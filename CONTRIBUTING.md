# Contributing

This project is a C++20 CMake codebase using Boost.Asio coroutines and GoogleTest. Contributions should keep changes focused, preserve the public protocol behavior unless an API change is intentional, and add tests for implemented behavior.

## Development Setup

Required tools and libraries:

- CMake 3.24 or newer.
- A C++20 compiler.
- Boost 1.83 or newer.
- GoogleTest for unit tests.
- `clang-format` for formatting checks.
- `clang-tidy` for static analysis checks.

Configure a build directory with tests enabled:

```sh
cmake --preset debug
```

## Code Style

The formatting source of truth is `.clang-format`:

- Based on LLVM style.
- 4-space indentation.
- 100-column line limit.
- No namespace indentation.
- Function opening braces go on the next line.
- Class, struct, enum, namespace, and control-statement opening braces stay on the same line.
- Includes are sorted.

Run the formatting check before merging:

```sh
cmake --build --preset debug --target clang-format-check
```

Static analysis is configured by `.clang-tidy`. The enabled checks include clang analyzer, bugprone, performance, portability, modernize, and readability checks, with project-specific exclusions for trailing return types, short identifiers, and magic numbers.

Run clang-tidy before merging:


```sh
cmake --build --preset debug --target clang-tidy
```

## Tests and Coverage Expectations

All behavior changes must be covered by tests. Add or update GoogleTest tests under `tests/unit` for:

- New public headers, types, and functions.
- Protocol encoding, decoding, validation, and error classification behavior.
- Transport and connection lifecycle behavior.
- Async request/response behavior, including failure paths.
- Bug fixes, with a regression test that fails without the fix.

There is currently no dedicated line-coverage target or minimum percentage gate in CMake. The merge requirement is behavior coverage: every implemented public behavior and every changed edge case must have a focused unit test, and the full unit test suite must pass.

Run the unit tests:

```sh
cmake --build --preset debug
ctest --preset debug
```

## Required Pre-Merge Checks

Before merging, the following checks must pass:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --build --preset debug --target clang-format-check
cmake --build --preset debug --target clang-tidy
```

Run the sanitizer presets for changes that affect memory ownership, concurrency, transport I/O, frame decoding, write queuing, or connection lifecycle:

```sh
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan

cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan
```

## Adding Files

Public library headers belong in `include/rillnet`. Unit tests belong in `tests/unit`, normally one test file per public header or behavior area.

When adding a new source, header, or test file, register it in the relevant CMake lists:

- Add format-checked files to `rillnet_format_sources` in `CMakeLists.txt`.
- Add clang-tidy-checked `.cpp` files through the same list.
- Add new unit test `.cpp` files to `tests/unit/CMakeLists.txt`.

Shared test-only headers should still be added to `rillnet_format_sources` so they are covered by formatting checks.

## Async and Error-Handling Conventions

- Prefer `boost::asio::awaitable` APIs for asynchronous operations.
- Report expected failures through result types such as `ConnectResult`, `EncodeResult`, `DecodeResult<T>`, and `WriteResult` rather than throwing.
- Keep transport and protocol failures mapped to `StatusCode` values.
- Preserve the single-writer rule for each connection: all frames should pass through `WriteQueue` instead of writing directly to a shared transport.
- Keep protocol wire formats explicit and deterministic; do not serialize protocol headers by copying struct memory.

## Documentation

Update `README.md` when implemented public behavior changes. The README should describe only behavior that exists in the current codebase.