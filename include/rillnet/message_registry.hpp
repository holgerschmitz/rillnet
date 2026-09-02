#pragma once

#include <rillnet/codec.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/protocol_version.hpp>

#include <cstdint>
#include <map>
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
    class VersionRegistry {
      public:
        template <typename Message>
        [[nodiscard]] MessageRegistrationResult register_message(std::uint32_t id)
        {
            return register_message<Message>(MessageType{id});
        }

        template <typename Message>
        [[nodiscard]] MessageRegistrationResult register_message(MessageType id)
        {
            return registry_.template register_message<Message>(version_, id);
        }

        template <typename Message> [[nodiscard]] bool contains() const
        {
            return registry_.template contains<Message>(version_);
        }

        [[nodiscard]] bool contains(MessageType id) const
        {
            return registry_.contains(version_, id);
        }

        template <typename Message> [[nodiscard]] std::optional<MessageType> message_type() const
        {
            return registry_.template message_type<Message>(version_);
        }

        [[nodiscard]] std::optional<std::type_index> message_type(MessageType id) const
        {
            return registry_.message_type(version_, id);
        }

      private:
        friend class MessageRegistry;

        VersionRegistry(MessageRegistry &registry, ProtocolVersion version) noexcept
            : registry_(registry), version_(version)
        {
        }

        MessageRegistry &registry_;
        ProtocolVersion version_;
    };

    [[nodiscard]] VersionRegistry for_version(ProtocolVersion version) noexcept
    {
        return VersionRegistry{*this, version};
    }

    template <typename Message>
    [[nodiscard]] MessageRegistrationResult register_message(std::uint32_t id)
    {
        return register_message<Message>(current_protocol_version, MessageType{id});
    }

    template <typename Message>
    [[nodiscard]] MessageRegistrationResult register_message(MessageType id)
    {
        return register_message<Message>(current_protocol_version, id);
    }

    template <typename Message>
    [[nodiscard]] MessageRegistrationResult register_message(ProtocolVersion version,
                                                             MessageType id)
    {
        if (!id.is_valid()) {
            return failure(MessageRegistrationError::invalid_id, "message id must be non-zero");
        }

        auto &registrations = registrations_[version];
        const std::type_index type = typeid(Message);
        if (registrations.by_id.contains(id)) {
            return failure(MessageRegistrationError::duplicate_id,
                           "message id is already registered");
        }
        if (registrations.by_type.contains(type)) {
            return failure(MessageRegistrationError::duplicate_type,
                           "message type is already registered");
        }
        if constexpr (!Codec<CodecType, Message>) {
            return failure(MessageRegistrationError::unsupported_message_type,
                           "message type is not supported by the registry codec");
        } else {
            registrations.by_id.emplace(id, type);
            registrations.by_type.emplace(type, id);
            return {};
        }
    }

    template <typename Message> [[nodiscard]] bool contains() const
    {
        return contains<Message>(current_protocol_version);
    }

    template <typename Message> [[nodiscard]] bool contains(ProtocolVersion version) const
    {
        const auto found = registrations_.find(version);
        return found != registrations_.end() &&
               found->second.by_type.contains(std::type_index(typeid(Message)));
    }

    [[nodiscard]] bool contains(MessageType id) const
    {
        return contains(current_protocol_version, id);
    }

    [[nodiscard]] bool contains(ProtocolVersion version, MessageType id) const
    {
        const auto found = registrations_.find(version);
        return found != registrations_.end() && found->second.by_id.contains(id);
    }

    template <typename Message> [[nodiscard]] std::optional<MessageType> message_type() const
    {
        return message_type<Message>(current_protocol_version);
    }

    template <typename Message>
    [[nodiscard]] std::optional<MessageType> message_type(ProtocolVersion version) const
    {
        const auto version_found = registrations_.find(version);
        if (version_found == registrations_.end()) {
            return std::nullopt;
        }
        const auto found = version_found->second.by_type.find(std::type_index(typeid(Message)));
        if (found == version_found->second.by_type.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::optional<std::type_index> message_type(MessageType id) const
    {
        return message_type(current_protocol_version, id);
    }

    [[nodiscard]] std::optional<std::type_index> message_type(ProtocolVersion version,
                                                              MessageType id) const
    {
        const auto version_found = registrations_.find(version);
        if (version_found == registrations_.end()) {
            return std::nullopt;
        }
        const auto found = version_found->second.by_id.find(id);
        if (found == version_found->second.by_id.end()) {
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

    struct VersionRegistrations {
        std::unordered_map<MessageType, std::type_index> by_id;
        std::unordered_map<std::type_index, MessageType> by_type;
    };

    std::map<ProtocolVersion, VersionRegistrations> registrations_;
};

} // namespace rillnet