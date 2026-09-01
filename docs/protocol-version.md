# Protocol Version

[`include/rillnet/protocol_version.hpp`](../include/rillnet/protocol_version.hpp)
defines `ProtocolVersion`, the type carried in every frame header to identify
the wire protocol version a frame was produced with.

## `ProtocolVersion`

```cpp
struct ProtocolVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
};
```

`ProtocolVersion` is a plain aggregate rather than a `StrongId`: unlike the
identifiers in [`identifiers.hpp`](../include/rillnet/identifiers.hpp), it is
not a single opaque handle but a pair of fields that are individually
meaningful and ordered (`major` before `minor`), so equality and ordering are
derived with a defaulted `operator<=>` over both fields directly.

`rillnet::current_protocol_version` is the version implemented by this
release (`0.1`). The initial implementation only ever speaks this one
version: a peer advertising any other version is a protocol error. 

## Usage example

```cpp
#include <rillnet/protocol_version.hpp>

if (received_version != rillnet::current_protocol_version) {
    // reject the connection with StatusCode::unsupported_version
}
```
