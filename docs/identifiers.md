# Protocol Identifiers

This document explains the identifier types introduced in Epic 1.2 of the
implementation plan: `StreamId`, `MessageType` and `RequestId`, all declared in
[`include/rillnet/identifiers.hpp`](../include/rillnet/identifiers.hpp).

## Why strong types instead of plain integers

Raw integers are easy to mix up: a stream identifier, a message type and a
byte count are all "just a number" until they are passed to the wrong
parameter and the compiler has no way to object. As the framework grows,
frame headers, routing tables and handler registries all pass these values
around, so a mistake is easy to make and hard to notice.

Each identifier is therefore represented by its own type, built from a small
`detail::StrongId<Tag, ValueType>` template. Two `StrongId` instantiations are
different types even when their underlying value type is the same, so
`StreamId` and `MessageType` cannot be implicitly converted into one another
or passed to the wrong function parameter. The wrapper is header-only,
trivially copyable and has no runtime overhead over the plain integer it
wraps.

Each `StrongId` supports:

- equality and ordering (via defaulted `operator<=>`), so identifiers can be
  stored in ordered containers such as `std::map` or `std::set`;
- `std::hash` support, so identifiers can be used as keys in
  `std::unordered_map`/`std::unordered_set`;
- an `is_valid()` check, with the zero value reserved to mean "no identifier
  assigned".

## `StreamId`

`StreamId` identifies a logical, multiplexed request/response exchange within
a single connection (see Epic 4.1/4.2). A connection can have many concurrent
streams in flight, and responses may complete in any order, so every frame
needs to carry the stream it belongs to.

`StreamId` wraps a `std::uint64_t`. A 64-bit value avoids wraparound concerns
for the lifetime of a connection, even under sustained high request rates.
Allocation rules (for example, separate ranges or odd/even numbering for
client- versus server-initiated streams) are the responsibility of the future
stream allocator (Epic 4.2) and are intentionally not encoded in the type
itself.

## `MessageType`

`MessageType` identifies the *kind* of message carried by a frame payload -
for example `StartSimulation` versus `SimulationStarted`. It is used by the
message registry (Epic 3.2) to map between wire values and C++ types, and by
servers to dispatch an incoming message to the correct handler.

`MessageType` wraps a `std::uint32_t`. Message types are a closed,
human-curated set defined by the application protocol, so a smaller value
range than `StreamId` is sufficient, while still leaving ample room for
protocols with many message kinds.

## `RequestId`

The implementation plan asks whether a request needs an identifier distinct
from its stream. In the v0.1 request/response model (Milestone 4), each
stream carries exactly one request and exactly one response, so the stream
identifier already uniquely identifies the request - a separate `RequestId`
type would only duplicate `StreamId`.

For this reason `RequestId` is currently defined as an alias of `StreamId`:

```cpp
using RequestId = StreamId;
```

This keeps call sites that talk about "the request" readable, without
introducing a second identifier that must always be kept equal to the first.
If a later milestone (for example server streaming, where one stream carries
many messages) requires multiple independently identified requests per
stream, `RequestId` can be turned into its own `StrongId` at that point
without affecting existing `StreamId` usage, since call sites already spell
the type as `RequestId` rather than `StreamId`.

## Usage example

```cpp
#include <rillnet/identifiers.hpp>

rillnet::StreamId stream{1};
rillnet::MessageType start_simulation{100};

if (stream.is_valid()) {
    // route the frame associated with `stream`
}

std::unordered_map<rillnet::StreamId, Operation> operations;
operations.emplace(stream, Operation{});
```
