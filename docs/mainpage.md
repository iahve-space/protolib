# protolib

# 🧠 Overview

**protolib** is a modern C++17 library for declarative definition, serialization, and parsing of binary communication protocols.  
It provides a zero-allocation, strongly typed approach to building and testing data exchange layers for both embedded and PC environments.

---

## ✨ Key Features

- **Per-field Endianness Control** — each field can independently specify *little-endian* or *big-endian* encoding.
- **Zero Dynamic Allocation** — all protocol data resides within a statically defined buffer; no heap memory is used during parsing or serialization.
- **Strict Type Safety** — every field explicitly defines its type and size at compile time, ensuring layout consistency and safety.
- **Incremental Parsing** — packets can be parsed progressively as data arrives (chunk-by-chunk), supporting streaming or UART-style communications.
- **Structured Packet Definition** — each protocol defines a clear data layout; array or vector fields are automatically recognized as contiguous buffers.
- **Debug-Friendly Tracing** — includes readable debug output for packet parsing and field inspection, simplifying communication debugging.
- **Payload Isolation** — payload data is logically separated from protocol metadata, but the full packet object (header + fields + CRC) is accessible for analysis.
- **Nested Protocols** — protocols can embed or encapsulate other protocols, supporting complex hierarchical message structures.
- **Custom Field Handlers** — extend behavior by defining custom CRCs or field parsers to tailor logic for specific protocol requirements.
- **Configurable Length and CRC Fields** — easily define which fields participate in *length* and *CRC* calculations.

---

## Protocol Architecture

Most binary transports share a recurring set of header fields that describe the message:

- `ID_FIELD` – protocol/message identifier  
- `TYPE_FIELD` – message subtype or operation code  
- `LEN_FIELD` – packet length or payload length  
- `SOURCE_FIELD` / `DEST_FIELD` – addressing / routing  
- `VERSION_FIELD` – protocol versioning  
- `CRC_FIELD` – integrity check (CRC/checksum)  
- …and others defined in your specification (see \ref proto::FieldName "FieldName").

`protolib` lets you compose these fields **in any order required** by your transport. Each field is strongly typed and may specify its own endianness. You declare the structure once and reuse it across targets.

---

### Containers and protocol reuse

A **container** is a reusable layout made of fields (header + payload descriptors).  
The **same container** can be applied **multiple times** across different protocols to avoid duplication.  
A single protocol may also define **distinct Rx and Tx containers** when the receive and transmit formats differ.

```
[Field] + [Field] + [Field]  →  Container
Container(Rx) + Container(Tx) → Protocol
```

Because containers are independent, you can keep shared header logic in one place and mix in protocol‑specific payloads where needed.

---

### Multiple protocols over one interface

It’s common to multiplex several protocols over a single byte stream (UART/TCP/SPI).  
You can instantiate multiple protocol parsers and **feed the same incoming stream** to each until one recognizes a valid packet:

```
            (byte stream)
[Interface] ───────────────► Demux
                 ├──► Protocol A (RxContainer)
                 ├──► Protocol B (RxContainer)
                 └──► Protocol C (RxContainer)
```

Each protocol consumes only the bytes it needs; partial data is supported through incremental parsing.

---

### CRC/length configuration

Length/CRC are often computed **only over specific ranges** (for example, from `TYPE_FIELD` to end of payload).  
Protocols can explicitly mark which fields participate in `LEN` / `ALEN` / `CRC` calculations and plug in **custom algorithms** (your own CRC or validator) via extensible field handlers.

---

### Testing with GoogleTest

After defining a protocol, you can validate it with **GoogleTest** by mocking the interface and asserting on parsed results:

```cpp
// Arrange: feed a byte stream into the protocol's Rx container
auto bytes = make_frame(/* header+payload with CRC */);
ProtocolA protoA;
auto consumed = protoA.feed(bytes);

// Assert: the packet was recognized and fields decoded
EXPECT_GT(consumed, 0);
EXPECT_EQ(protoA.rx().id.value(), 0x42);
// If using matchers:
EXPECT_THAT(protoA.rx().payload, ::testing::SizeIs(8));
```

This approach enables fast, deterministic tests for both serialization and parsing, and keeps protocol logic decoupled from I/O.


---

## 📚 Documentation Structure

- @ref api "API Reference" — classes, fields, and serialization methods
- @ref protocols "Protocol Implementations" — defined packet layouts
- @ref examples "Usage Examples" — minimal examples and integration snippets
- @ref integration "Integration with CMake and Conan" — build system notes

---

## 🧩 Integration

### Using CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
  protolib
  GIT_REPOSITORY https://github.com/iahve-space/protolib.git
  GIT_TAG main
)
FetchContent_MakeAvailable(protolib)

target_link_libraries(your_app PRIVATE proto)
```

### Using Conan

```bash
conan create . --version="${VERSION}" -s:h build_type=Release
```

and in your `CMakeLists.txt`:

```cmake
find_package(protolib REQUIRED)
target_link_libraries(your_app PRIVATE proto::protolib)
```

---

## 🧭 Supported Platforms

| Platform | Compiler | Status |
|-----------|-----------|---------|
| Linux | GCC / Clang | ✅ |
| macOS | Clang | ✅ |
| Windows | MinGW / MSVC | ⚙️ |
| Embedded (NuttX / STM32) | Cross-compile via `BUILD_FOR_TARGET` | 🔧 |

> ⚠️ Some usage differences may apply on embedded targets — see the [Wiki](https://github.com/iahve-space/protolib/wiki) for details.

---

## 🧱 Project Structure

```
include/        # Public headers (core API)
protocols/      # Protocol definitions and tests
libraries/      # Internal implementation
tests/          # Unit tests (GoogleTest)
docs/           # Documentation (Doxygen + Pages)
```

---

## 🧩 Repository & License

- **Repository:** [github.com/iahve-space/protolib](https://github.com/iahve-space/protolib)
- **License:** GNU GPL v3
- **Maintainer:** [iahve-space](https://github.com/iahve-space)

---

## ❤️ Acknowledgements

This project is part of the **iahve-space** infrastructure —  
a unified build ecosystem based on **CMake + Conan**,  
focused on **embedded development**, **cross-compilation**, and **reproducible builds**.

---

<sub>© 2025 iahve-space. Built with ❤️ and C++17.</sub>
