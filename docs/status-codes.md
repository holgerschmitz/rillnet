# Status Codes

[`include/rillnet/status_code.hpp`](../include/rillnet/status_code.hpp)
defines `StatusCode`, the wire-visible outcome code for a frame, stream or
connection, and `StatusCategory`, the broad category a `StatusCode` belongs
to.

## `StatusCode`

`StatusCode` is grouped into fixed, 1000-wide numeric bands, one per
category:

| Band | Category        | Example codes                                              |
|-----:|-----------------|-------------------------------------------------------------|
|    0 | ok               | `ok`                                                        |
| 1000 | transport        | `connection_closed`, `connection_reset`                     |
| 2000 | protocol         | `unsupported_version`, `malformed_frame`, `unknown_stream`   |
| 3000 | serialization    | `unknown_message_type`, `decode_error`                       |
| 4000 | operation        | `operation_error`                                            |
| 5000 | timeout          | `timeout_error`                                              |
| 6000 | cancellation     | `cancelled`                                                  |
| 7000 | resource_limit   | `resource_limit_exceeded`                                    |

Banding by 1000 lets new, more specific codes be added within a category
later without renumbering existing ones, and lets `status_category` recover
a code's category purely from its numeric value (`code / 1000`), which
matters because that value is what actually crosses the wire.

`is_error(code)` is a convenience for `code != StatusCode::ok`, and
`to_string(code)` returns a stable, human-readable name for logging and
diagnostics.

## Usage example

```cpp
#include <rillnet/status_code.hpp>

rillnet::StatusCode code = rillnet::StatusCode::unknown_stream;

if (rillnet::is_error(code)) {
    log("frame rejected: {} ({})", rillnet::to_string(code),
        rillnet::status_category(code));
}
```
