#include <rillnet/frame_codec.hpp>
#include <rillnet/message_codec.hpp>
#include <rillnet/message_registry.hpp>
#include <rillnet/server_connection.hpp>

#include "in_memory_transport.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using rillnet::decode_message;
using rillnet::encode_frame;
using rillnet::encode_message;
using rillnet::Frame;
using rillnet::FrameDecoder;
using rillnet::FrameType;
using rillnet::MessageRegistry;
using rillnet::ServerConnection;
using rillnet::SessionContext;
using rillnet::StreamId;
using rillnet::testing::InMemoryTransport;

struct StartSimulation {
    std::uint32_t id = 0;
};

struct SimulationStarted {
    std::uint32_t id = 0;
};

MessageRegistry<> make_registry()
{
    MessageRegistry<> registry;
    EXPECT_TRUE(registry.register_message<StartSimulation>(100).ok());
    EXPECT_TRUE(registry.register_message<SimulationStarted>(101).ok());
    return registry;
}

std::vector<std::byte> encode_request_bytes(const MessageRegistry<> &registry)
{
    std::vector<std::byte> bytes;
    for (const auto &[stream, id] : {std::pair{StreamId{1}, 7U}, std::pair{StreamId{3}, 9U}}) {
        const auto encoded = encode_message(registry, StartSimulation{id}, stream);
        EXPECT_TRUE(encoded.ok());
        const auto frame_bytes =
            encode_frame(*encoded.frame); // NOLINT(bugprone-unchecked-optional-access)
        bytes.insert(bytes.end(), frame_bytes.begin(), frame_bytes.end());
    }
    return bytes;
}

TEST(ServerConnectionTest, DispatchesHandlersWithoutBlockingTheReadLoop)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    auto transport = std::make_unique<InMemoryTransport>(encode_request_bytes(registry));
    auto *transport_ptr = transport.get();
    ServerConnection connection(context.get_executor(), std::move(transport), registry);

    connection.handle<StartSimulation>(
        [&context](SessionContext &session,
                   StartSimulation request) -> boost::asio::awaitable<SimulationStarted> {
            if (request.id == 7) {
                boost::asio::steady_timer timer(context);
                timer.expires_after(std::chrono::milliseconds(1));
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            co_return SimulationStarted{request.id +
                                        static_cast<std::uint32_t>(session.stream_id().value())};
        });

    auto future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);
    context.run();
    future.get();

    FrameDecoder decoder;
    const auto responses = decoder.push(transport_ptr->outgoing());
    ASSERT_EQ(responses.size(), 2U);
    EXPECT_EQ(responses[0].header.type, FrameType::response);
    EXPECT_EQ(responses[0].header.stream, StreamId{3});
    EXPECT_EQ(responses[1].header.type, FrameType::response);
    EXPECT_EQ(responses[1].header.stream, StreamId{1});

    const auto first_response = decode_message<SimulationStarted>(registry, responses[0]);
    const auto second_response = decode_message<SimulationStarted>(registry, responses[1]);
    ASSERT_TRUE(first_response.ok());
    ASSERT_TRUE(second_response.ok());
    EXPECT_EQ(first_response.value->id, 12U); // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(second_response.value->id, 8U); // NOLINT(bugprone-unchecked-optional-access)
}

} // namespace