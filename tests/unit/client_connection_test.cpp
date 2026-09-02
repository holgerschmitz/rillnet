#include <rillnet/client_connection.hpp>

#include "in_memory_transport.hpp"

#include <rillnet/frame.hpp>
#include <rillnet/frame_codec.hpp>
#include <rillnet/frame_decoder.hpp>
#include <rillnet/frame_flags.hpp>
#include <rillnet/message_codec.hpp>
#include <rillnet/message_registry.hpp>
#include <rillnet/status_code.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {

using rillnet::ClientConnection;
using rillnet::DecodeResult;
using rillnet::encode_frame;
using rillnet::encode_message;
using rillnet::FrameDecoder;
using rillnet::FrameFlags;
using rillnet::FrameType;
using rillnet::has_flag;
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

std::vector<std::byte>
encode_response_bytes(const MessageRegistry<> &registry,
                      std::initializer_list<std::pair<StreamId, SimulationStarted>> responses)
{
    std::vector<std::byte> bytes;
    for (const auto &[stream, response] : responses) {
        const auto encoded = encode_message(registry, response, stream, FrameType::response);
        EXPECT_TRUE(encoded.ok());
        const auto frame_bytes =
            encode_frame(*encoded.frame); // NOLINT(bugprone-unchecked-optional-access)
        bytes.insert(bytes.end(), frame_bytes.begin(), frame_bytes.end());
    }
    return bytes;
}

TEST(ClientConnectionTest, CorrelatesResponsesWhenTheyCompleteOutOfOrder)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    auto transport = std::make_unique<InMemoryTransport>(encode_response_bytes(
        registry, {{StreamId{3}, {30}}, {StreamId{1}, {10}}, {StreamId{5}, {50}}}));
    ClientConnection connection(context.get_executor(), std::move(transport), registry);

    auto first_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({1});
        },
        boost::asio::use_future);
    auto second_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({2});
        },
        boost::asio::use_future);
    auto third_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({3});
        },
        boost::asio::use_future);
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    const auto first_result = first_request.get();
    const auto second_result = second_request.get();
    const auto third_result = third_request.get();
    run_future.get();

    ASSERT_TRUE(first_result.ok());
    ASSERT_TRUE(second_result.ok());
    ASSERT_TRUE(third_result.ok());
    ASSERT_TRUE(first_result.value.has_value());
    ASSERT_TRUE(second_result.value.has_value());
    ASSERT_TRUE(third_result.value.has_value());
    EXPECT_EQ(first_result.value->id, 10U);  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(second_result.value->id, 30U); // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(third_result.value->id, 50U);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ClientConnectionTest, CompletesHundredsOfConcurrentOperations)
{
    constexpr std::uint32_t operation_count = 256;
    boost::asio::io_context context;
    const auto registry = make_registry();
    std::vector<std::byte> incoming;
    for (std::uint32_t index = 0; index < operation_count; ++index) {
        const auto stream = StreamId{1 + (index * 2)};
        const auto encoded =
            encode_message(registry, SimulationStarted{index}, stream, FrameType::response);
        EXPECT_TRUE(encoded.ok());
        const auto frame_bytes =
            encode_frame(*encoded.frame); // NOLINT(bugprone-unchecked-optional-access)
        incoming.insert(incoming.end(), frame_bytes.begin(), frame_bytes.end());
    }
    ClientConnection connection(context.get_executor(),
                                std::make_unique<InMemoryTransport>(std::move(incoming)), registry);

    std::vector<std::future<DecodeResult<SimulationStarted>>> requests;
    requests.reserve(operation_count);
    for (std::uint32_t index = 0; index < operation_count; ++index) {
        requests.push_back(boost::asio::co_spawn(
            context,
            [&connection, index]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
                co_return co_await connection.request<StartSimulation, SimulationStarted>({index});
            },
            boost::asio::use_future));
    }
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    for (std::uint32_t index = 0; index < operation_count; ++index) {
        const auto result = requests[index].get();
        ASSERT_TRUE(result.ok());
        ASSERT_TRUE(result.value.has_value());
        EXPECT_EQ(result.value->id, index); // NOLINT(bugprone-unchecked-optional-access)
    }
    run_future.get();
}

TEST(ClientConnectionTest, SupportsMultiThreadedIoWithSerializedConnectionState)
{
    constexpr std::uint32_t operation_count = 128;
    boost::asio::io_context context;
    auto connection_executor = boost::asio::make_strand(context);
    const auto registry = make_registry();
    std::vector<std::byte> incoming;
    for (std::uint32_t index = 0; index < operation_count; ++index) {
        const auto encoded = encode_message(registry, SimulationStarted{index},
                                            StreamId{1 + (index * 2)}, FrameType::response);
        EXPECT_TRUE(encoded.ok());
        const auto frame_bytes =
            encode_frame(*encoded.frame); // NOLINT(bugprone-unchecked-optional-access)
        incoming.insert(incoming.end(), frame_bytes.begin(), frame_bytes.end());
    }
    ClientConnection connection(connection_executor,
                                std::make_unique<InMemoryTransport>(std::move(incoming)), registry);

    std::vector<std::future<DecodeResult<SimulationStarted>>> requests;
    requests.reserve(operation_count);
    for (std::uint32_t index = 0; index < operation_count; ++index) {
        requests.push_back(boost::asio::co_spawn(
            connection_executor,
            [&connection, index]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
                co_return co_await connection.request<StartSimulation, SimulationStarted>({index});
            },
            boost::asio::use_future));
    }
    auto run_future = boost::asio::co_spawn(
        connection_executor, [&]() { return connection.run(); }, boost::asio::use_future);

    std::vector<std::thread> workers;
    for (int index = 0; index < 4; ++index) {
        workers.emplace_back([&context]() { context.run(); });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    for (std::uint32_t index = 0; index < operation_count; ++index) {
        const auto result = requests[index].get();
        ASSERT_TRUE(result.ok());
        ASSERT_TRUE(result.value.has_value());
        EXPECT_EQ(result.value->id, index); // NOLINT(bugprone-unchecked-optional-access)
    }
    run_future.get();
}

TEST(ClientConnectionTest, FailsOneOperationWithoutAffectingOtherOperations)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    const auto failed_response =
        encode_message(registry, StartSimulation{11}, StreamId{1}, FrameType::response);
    EXPECT_TRUE(failed_response.ok());
    auto incoming =
        encode_frame(*failed_response.frame); // NOLINT(bugprone-unchecked-optional-access)
    const auto successful_responses =
        encode_response_bytes(registry, {{StreamId{3}, {22}}, {StreamId{5}, {33}}});
    incoming.insert(incoming.end(), successful_responses.begin(), successful_responses.end());
    ClientConnection connection(context.get_executor(),
                                std::make_unique<InMemoryTransport>(std::move(incoming)), registry);

    auto first_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({1});
        },
        boost::asio::use_future);
    auto second_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({2});
        },
        boost::asio::use_future);
    auto third_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({3});
        },
        boost::asio::use_future);
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    const auto first_result = first_request.get();
    const auto second_result = second_request.get();
    const auto third_result = third_request.get();
    run_future.get();
    EXPECT_FALSE(first_result.ok());
    EXPECT_EQ(first_result.status, StatusCode::unknown_message_type);
    ASSERT_TRUE(second_result.ok());
    ASSERT_TRUE(second_result.value.has_value());
    ASSERT_TRUE(third_result.ok());
    ASSERT_TRUE(third_result.value.has_value());
    EXPECT_EQ(second_result.value->id, 22U); // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(third_result.value->id, 33U);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ClientConnectionTest, IgnoresRequestFramesWhileAwaitingAResponse)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    const auto encoded = encode_message(registry, StartSimulation{99}, StreamId{1});
    EXPECT_TRUE(encoded.ok());
    auto transport = std::make_unique<InMemoryTransport>(
        encode_frame(*encoded.frame)); // NOLINT(bugprone-unchecked-optional-access)
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

    run_future.get();
    const auto result = request_future.get();
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status, StatusCode::connection_closed);
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

TEST(ClientConnectionTest, FailsAllOutstandingOperationsWhenTheTransportCloses)
{
    constexpr std::uint32_t operation_count = 128;
    boost::asio::io_context context;
    const auto registry = make_registry();
    ClientConnection connection(context.get_executor(),
                                std::make_unique<InMemoryTransport>(std::vector<std::byte>{}),
                                registry);

    std::vector<std::future<DecodeResult<SimulationStarted>>> requests;
    requests.reserve(operation_count);
    for (std::uint32_t index = 0; index < operation_count; ++index) {
        requests.push_back(boost::asio::co_spawn(
            context,
            [&connection, index]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
                co_return co_await connection.request<StartSimulation, SimulationStarted>({index});
            },
            boost::asio::use_future));
    }
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    for (auto &request : requests) {
        const auto result = request.get();
        EXPECT_FALSE(result.ok());
        EXPECT_EQ(result.status, StatusCode::connection_closed);
    }
    run_future.get();
}

TEST(ClientConnectionTest, IgnoresDuplicateResponsesAndResponsesForUnknownStreams)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    auto incoming = encode_response_bytes(
        registry, {{StreamId{999}, {99}}, {StreamId{1}, {42}}, {StreamId{1}, {43}}});
    ClientConnection connection(context.get_executor(),
                                std::make_unique<InMemoryTransport>(std::move(incoming)), registry);

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

TEST(ClientConnectionTest, CancelsOneOperationWithoutClosingTheConnection)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    auto transport =
        std::make_unique<InMemoryTransport>(encode_response_bytes(registry, {{StreamId{3}, {22}}}));
    const auto *transport_observer = transport.get();
    ClientConnection connection(context.get_executor(), std::move(transport), registry);

    auto cancelled_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            auto started =
                co_await connection.start_request<StartSimulation, SimulationStarted>({1});
            if (!started.ok()) {
                co_return DecodeResult<SimulationStarted>::failure(started.status, started.message);
            }

            auto operation = std::move(*started.operation);
            EXPECT_EQ(operation.stream(), StreamId{1});
            EXPECT_TRUE(operation.cancel());
            co_return co_await operation.async_wait();
        },
        boost::asio::use_future);
    auto successful_request = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            co_return co_await connection.request<StartSimulation, SimulationStarted>({2});
        },
        boost::asio::use_future);
    auto run_future =
        boost::asio::co_spawn(context, [&]() { return connection.run(); }, boost::asio::use_future);

    context.run();

    const auto cancelled_result = cancelled_request.get();
    const auto successful_result = successful_request.get();
    run_future.get();

    EXPECT_FALSE(cancelled_result.ok());
    EXPECT_EQ(cancelled_result.status, StatusCode::cancelled);
    EXPECT_FALSE(cancelled_result.value.has_value());
    ASSERT_TRUE(successful_result.ok());
    ASSERT_TRUE(successful_result.value.has_value());
    EXPECT_EQ(successful_result.value->id, 22U); // NOLINT(bugprone-unchecked-optional-access)

    FrameDecoder outgoing_decoder;
    auto outgoing_frames = outgoing_decoder.push(transport_observer->outgoing());
    ASSERT_EQ(outgoing_frames.size(), 3U);
    EXPECT_EQ(outgoing_frames[1].header.stream, StreamId{1});
    EXPECT_EQ(outgoing_frames[1].header.type, FrameType::request);
    EXPECT_TRUE(has_flag(outgoing_frames[1].header.flags, FrameFlags::cancel));
}

TEST(ClientConnectionTest, ResponseWinsWhenItArrivesBeforeCancellation)
{
    boost::asio::io_context context;
    const auto registry = make_registry();
    ClientConnection connection(
        context.get_executor(),
        std::make_unique<InMemoryTransport>(encode_response_bytes(registry, {42})), registry);

    auto request_future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<DecodeResult<SimulationStarted>> {
            auto started =
                co_await connection.start_request<StartSimulation, SimulationStarted>({7});
            if (!started.ok()) {
                co_return DecodeResult<SimulationStarted>::failure(started.status, started.message);
            }

            auto operation = std::move(*started.operation);
            co_await boost::asio::post(boost::asio::use_awaitable);
            EXPECT_FALSE(operation.cancel());
            co_return co_await operation.async_wait();
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

} // namespace
