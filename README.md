# urkern

Foundational primitives shared across teoncreative projects.

Named from German *Urkern* "primal core." Intentionally minimal: a home for
pieces that sit under everything else and that we'd otherwise re-implement in
each project.

## Scope

- `urkern/buffer.h`: typed binary buffer with endianness-aware read/write, varints, seal, etc.
- `urkern/uuid.h`: 128-bit UUID (v4 generation + parse/format).
- `urkern/platform_detection.h`: compile-time OS macros (`URKERN_PLATFORM_WINDOWS`, `URKERN_PLATFORM_LINUX`, etc.).

## Use

Header-only except for `uuid.cc`. Consume as a CMake subdirectory:

```cmake
add_subdirectory(vendor/urkern)
target_link_libraries(my_target PUBLIC urkern)
```

## License

Apache 2.0.
