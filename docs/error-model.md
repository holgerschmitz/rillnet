# Protocol Error Model

`StatusCode` identifies a failure's category and wire-visible code. `error_disposition` defines
the corresponding lifecycle and reporting policy in one place.

| Category | Affected scope | Sent to peer | Local only |
| --- | --- | --- | --- |
| Transport | Connection | No | Yes |
| Protocol | Connection | No | Yes |
| Serialization | Operation | Yes | No |
| Operation | Operation | Yes | No |
| Timeout | Operation | Yes | No |
| Cancellation | Operation | Yes | No |
| Resource limit | Operation | Yes | No |

`ok` affects neither an operation nor a connection. A connection-scoped error terminates every
active operation on that connection. A local-only error must not attempt to write a protocol error
frame because the transport is unavailable or the peer has violated the framing contract.

Operation-scoped errors are eligible for a structured error response on their stream when the
request/response layer is added. Eligibility does not require a response if the operation has
already ended or the connection has closed.