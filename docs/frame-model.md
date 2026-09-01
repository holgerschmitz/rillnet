# Frame Model

[`include/rillnet/frame.hpp`](../include/rillnet/frame.hpp) defines the logical frame exchanged by
the protocol layer. It deliberately does not define byte order, field offsets, header width, or
socket read behavior; those belong to frame encoding and incremental decoding.

## Logical representation

`FrameHeader` carries the protocol version, frame role, flags, logical stream identifier, and
declared payload size. `Frame` owns that header and its payload bytes in a `Buffer`.

The initial protocol recognizes request and response frame types. Numeric `FrameType` values are
wire-visible and closed for a protocol version, so peers must reject unknown values rather than
silently interpreting them.

## Validation

`validate_frame` checks an untrusted logical frame and returns a `FrameValidationError` instead of
throwing. A valid frame must:

- use `current_protocol_version`;
- have a known frame type and only known flag bits;
- not combine `cancel` and `error`, because cancellation control and an error payload have
  different meanings;
- identify a nonzero logical stream;
- declare a payload no larger than the configured limit; and
- have a declared payload size exactly matching the owned payload.

The default payload limit is 16 MiB and callers may supply a lower connection-specific limit.
Payload-limit validation occurs before comparing the declared length to allocated bytes so an
incremental decoder can reject oversized declarations before allocating payload storage.