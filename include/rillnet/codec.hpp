#pragma once

#include <rillnet/frame.hpp>
#include <rillnet/status_code.hpp>

#include <concepts>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace rillnet {

// The result of encoding a value into a wire payload.
struct EncodeResult {
    std::optional<Buffer> payload;
    StatusCode status = StatusCode::ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == StatusCode::ok; }

    [[nodiscard]] static EncodeResult success(Buffer payload) noexcept
    {
        return EncodeResult{std::move(payload), StatusCode::ok, {}};
    }

    [[nodiscard]] static EncodeResult failure(StatusCode status, std::string message)
    {
        return EncodeResult{std::nullopt, status, std::move(message)};
    }
};

// The result of decoding a wire payload into a value of type T.
template <typename T> struct DecodeResult {
    std::optional<T> value;
    StatusCode status = StatusCode::ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == StatusCode::ok; }

    [[nodiscard]] static DecodeResult
    success(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        return DecodeResult{std::move(value), StatusCode::ok, {}};
    }

    [[nodiscard]] static DecodeResult failure(StatusCode status, std::string message)
    {
        return DecodeResult{std::nullopt, status, std::move(message)};
    }
};

// A serialization technology capable of turning values of type T into wire payloads and back.
// Codecs are stateless types selected by applications when registering messages; the
// framework itself never assumes one specific serialization technology.
//
// encode/decode report failure through EncodeResult/DecodeResult rather than exceptions, so a
// codec-specific failure can always be mapped into StatusCategory::serialization uniformly,
// regardless of which Codec implementation produced it.
template <typename C, typename T>
concept Codec = requires(const T &value, const Buffer &payload) {
    { C::encode(value) } -> std::same_as<EncodeResult>;
    { C::template decode<T>(payload) } -> std::same_as<DecodeResult<T>>;
};

// A minimal built-in codec suitable for examples and tests. It encodes a trivially copyable
// message type as its raw in-memory representation, with no attempt at a stable cross-platform
// or cross-compiler wire format (no endianness handling, no support for pointers or containers).
// Applications with real interoperability requirements are expected to supply their own Codec.
struct PodCodec {
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] static EncodeResult encode(const T &value)
    {
        Buffer payload(sizeof(T));
        std::memcpy(payload.data(), &value, sizeof(T));
        return EncodeResult::success(std::move(payload));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] static DecodeResult<T> decode(const Buffer &payload)
    {
        if (payload.size() != sizeof(T)) {
            return DecodeResult<T>::failure(StatusCode::decode_error,
                                            "payload size does not match decoded type");
        }
        T value;
        std::memcpy(&value, payload.data(), sizeof(T));
        return DecodeResult<T>::success(std::move(value));
    }
};

} // namespace rillnet
