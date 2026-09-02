#include <rillnet/client_connection.hpp>

#include "in_memory_transport.hpp"

#include <rillnet/frame.hpp>
#include <rillnet/frame_codec.hpp>
#include <rillnet/message_codec.hpp>
#include <rillnet/message_registry.hpp>
#include <rillnet/status_code.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using rillnet::ClientConnection;
using rillnet::DecodeResult;
using rillnet::encode_frame;
using rillnet::encode_message;
using rillnet::FrameType;
using rillnet::MessageRegistry;
using rillnet::StatusCode;
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

// A client's first allocated stream id is always 1 (see StreamIdAllocator), so a canned response
// can be built for it up front.
std::vector<std::byte> encode_response_bytes(const MessageRegistry<> &registry,
                                             SimulationStarted response)
{
    const auto encoded = encode_message(registry, response, StreamId{1}, FrameType::response);
    EXPECT_TRUE(encoded.ok());
    return encode_frame(*encoded.frame); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ClientConnectionTest, RequestSendsAFrameAndDecodesTheMatchingResponse)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    auto transport = std::make_unique<InMemoryTransport>(encode_response_bytes(registry, {42}));
    ClientConnection connection(context.get_executor(), std::move(transport), registry);

    auto request_future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({7});
        },
        boost::asio::use_future);
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    const auto result = request_future.get();
    run_future.get();
    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value->id, 42U); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ClientConnectionTest, RequestFailsImmediatelyForAnUnregisteredMessageType)
{
    boost::asio::io_context context;
    const MessageRegistry<> registry; // StartSimulation intentionally not registered
    ClientConnection connection(context.get_executor(),
                                std::make_unique<InMemoryTransport>(std::vector<std::byte>{}),
                                registry);

    auto request_future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({7});
        },
        boost::asio::use_future);

    context.run();

    const auto result = request_future.get();
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status, StatusCode::unknown_message_type);
}

TEST(ClientConnectionTest, RequestFailsWithConnectionClosedWhenTheTransportClosesFirst)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    ClientConnection connection(context.get_executor(),
                                std::make_unique<InMemoryTransport>(std::vector<std::byte>{}),
                                registry);

    // Spawned before run(), so the request has already registered itself as pending by the time
    // the read loop observes end-of-stream and fails every outstanding request.
    auto request_future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({7});
        },
        boost::asio::use_future);
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    run_future.get();
    const auto result = request_future.get();
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status, StatusCode::connection_closed);
}

} // namespace
