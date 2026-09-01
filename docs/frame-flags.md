# Frame Flags

[`include/rillnet/frame_flags.hpp`](../include/rillnet/frame_flags.hpp)
defines `FrameFlags`, a bit-flag enum carried in every frame header that
describes how a frame should be handled independently of its payload bytes.

## `FrameFlags`

```cpp
enum class FrameFlags : std::uint8_t {
    none          = 0,
    end_of_stream = 1U << 0U,
    cancel        = 1U << 1U,
    error         = 1U << 2U,
};
```

- `end_of_stream` marks the terminal frame for its stream; the sender will
  not emit further frames for that stream afterwards. A peer that receives a
  second terminal frame for the same stream has observed a protocol
  violation.
- `cancel` requests or acknowledges cancellation of the operation associated
  with the frame's stream.
- `error` indicates that the frame's payload is a structured `StatusCode`
  rather than an ordinary application message.

These three flags cover the v0.1 request/response model. Additional flags can
be added within the remaining bits of the byte as later milestones (streaming,
flow control) require them.

## Why a scoped enum instead of a plain bitmask

`FrameFlags` is a normal `enum class`, which does not get bitwise operators
for free. [`frame_flags.hpp`](../include/rillnet/frame_flags.hpp) defines
`operator|`, `operator&`, `operator~`, `operator|=`, `operator&=` and a
`has_flag` helper so flags can be combined and tested with normal syntax
while keeping `FrameFlags` a distinct type that cannot be mixed up with an
unrelated integer or another enum.

Unknown bits received from a peer are not silently ignored.

## Usage example

```cpp
#include <rillnet/frame_flags.hpp>

auto flags = rillnet::FrameFlags::none;
flags |= rillnet::FrameFlags::end_of_stream;

if (rillnet::has_flag(flags, rillnet::FrameFlags::end_of_stream)) {
    // no further frames are expected for this stream
}
```
