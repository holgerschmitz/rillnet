#pragma once

#include <rillnet/codec.hpp>
#include <rillnet/identifiers.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace rillnet {

enum class MessageRegistrationError : std::uint8_t {
    none,
    invalid_id,
    duplicate_id,
    duplicate_type,
    unsupported_message_type,
};

struct MessageRegistrationResult {
    MessageRegistrationError error = MessageRegistrationError::none;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return error == MessageRegistrationError::none; }
};

// Maps application message types to their wire identifiers for one serialization codec.
template <typename CodecType = PodCodec> class MessageRegistry {
  public:
    template <typename Message>
    [[nodiscard]] MessageRegistrationResult register_message(std::uint32_t id)
    {
        return register_message<Message>(MessageType{id});
    }

    template <typename Message>
    [[nodiscard]] MessageRegistrationResult register_message(MessageType id)
    {
        if (!id.is_valid()) {
            return failure(MessageRegistrationError::invalid_id, "message id must be non-zero");
        }

        const std::type_index type = typeid(Message);
        if (by_id_.contains(id)) {
            return failure(MessageRegistrationError::duplicate_id,
                           "message id is already registered");
        }
        if (by_type_.contains(type)) {
            return failure(MessageRegistrationError::duplicate_type,
                           "message type is already registered");
        }
        if constexpr (!Codec<CodecType, Message>) {
            return failure(MessageRegistrationError::unsupported_message_type,
                           "message type is not supported by the registry codec");
        } else {
            by_id_.emplace(id, type);
            by_type_.emplace(type, id);
            return {};
        }
    }

    template <typename Message> [[nodiscard]] bool contains() const
    {
        return by_type_.contains(std::type_index(typeid(Message)));
    }

    [[nodiscard]] bool contains(MessageType id) const { return by_id_.contains(id); }

    template <typename Message> [[nodiscard]] std::optional<MessageType> message_type() const
    {
        const auto found = by_type_.find(std::type_index(typeid(Message)));
        if (found == by_type_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::optional<std::type_index> message_type(MessageType id) const
    {
        const auto found = by_id_.find(id);
        if (found == by_id_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

  private:
    [[nodiscard]] static MessageRegistrationResult failure(MessageRegistrationError error,
                                                           std::string message)
    {
        return MessageRegistrationResult{error, std::move(message)};
    }

    std::unordered_map<MessageType, std::type_index> by_id_;
    std::unordered_map<std::type_index, MessageType> by_type_;
};

} // namespace rillnet