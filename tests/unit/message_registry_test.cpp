#include <rillnet/message_registry.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <typeindex>

namespace {

struct StartSimulation {
    std::uint32_t id = 0;
};

struct SimulationStarted {
    std::uint32_t id = 0;
};

struct UnsupportedMessage {
    std::string value;
};

using rillnet::MessageRegistrationError;
using rillnet::MessageRegistry;
using rillnet::MessageType;

TEST(MessageRegistryTest, RegistersAndLooksUpMessageInBothDirections)
{
    MessageRegistry registry;

    const auto result = registry.register_message<StartSimulation>(100);

    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(registry.contains<StartSimulation>());
    EXPECT_TRUE(registry.contains(MessageType{100}));
    ASSERT_TRUE(registry.message_type<StartSimulation>().has_value());
    EXPECT_EQ(*registry.message_type<StartSimulation>(), MessageType{100});
    ASSERT_TRUE(registry.message_type(MessageType{100}).has_value());
    EXPECT_EQ(*registry.message_type(MessageType{100}), std::type_index(typeid(StartSimulation)));
}

TEST(MessageRegistryTest, RejectsDuplicateIdsWithoutChangingTheExistingRegistration)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());

    const auto result = registry.register_message<SimulationStarted>(100);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, MessageRegistrationError::duplicate_id);
    EXPECT_FALSE(registry.contains<SimulationStarted>());
    EXPECT_EQ(registry.message_type<StartSimulation>(), std::optional{MessageType{100}});
}

TEST(MessageRegistryTest, RejectsDuplicateTypes)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());

    const auto result = registry.register_message<StartSimulation>(101);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, MessageRegistrationError::duplicate_type);
    EXPECT_FALSE(registry.contains(MessageType{101}));
}

TEST(MessageRegistryTest, RejectsInvalidIds)
{
    MessageRegistry registry;

    const auto result = registry.register_message<StartSimulation>(0);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, MessageRegistrationError::invalid_id);
}

TEST(MessageRegistryTest, RejectsMessageTypesUnsupportedByTheCodec)
{
    MessageRegistry registry;

    const auto result = registry.register_message<UnsupportedMessage>(100);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, MessageRegistrationError::unsupported_message_type);
    EXPECT_FALSE(registry.contains(MessageType{100}));
}

} // namespace