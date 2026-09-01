#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace rillnet {

namespace detail {

// A zero-overhead wrapper that gives an integer value a distinct type so that
// identifiers of different kinds cannot be mixed up or implicitly converted.
template <typename Tag, typename ValueType> class StrongId {
  public:
    using value_type = ValueType;

    constexpr StrongId() noexcept = default;
    constexpr explicit StrongId(value_type value) noexcept : value_(value) {}

    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != invalid_value; }

    friend constexpr auto operator<=>(const StrongId &, const StrongId &) = default;

  private:
    static constexpr value_type invalid_value = value_type{0};

    value_type value_ = invalid_value;
};

} // namespace detail

struct StreamIdTag {};

struct MessageTypeTag {};

// Identifies a logical, multiplexed request/response exchange within a connection.
using StreamId = detail::StrongId<StreamIdTag, std::uint64_t>;

// Identifies the type of a protocol message, used for registration, dispatch and serialization.
using MessageType = detail::StrongId<MessageTypeTag, std::uint32_t>;

// The v0.1 request/response model has exactly one request per stream, so the stream identifier
// also serves as the request identifier. A distinct type is not introduced until a future
// milestone requires more than one request per stream.
using RequestId = StreamId;

} // namespace rillnet

template <typename Tag, typename ValueType>
struct std::hash<rillnet::detail::StrongId<Tag, ValueType>> {
    std::size_t operator()(const rillnet::detail::StrongId<Tag, ValueType> &id) const noexcept
    {
        return std::hash<ValueType>{}(id.value());
    }
};
