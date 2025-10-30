# protolib

> A lightweight C++17 library for describing, serializing, and parsing binary protocols across embedded and desktop environments.

<!-- ✅ GitHub CI -->
[![X86, Build, Test, Coverage & Badge](https://github.com/iahve-space/protolib/actions/workflows/x86.yml/badge.svg)](https://github.com/iahve-space/protolib/actions/workflows/x86.yml)
[![Release](https://img.shields.io/github/v/release/iahve-space/protolib?include_prereleases&label=release)](https://github.com/iahve-space/protolib/releases)
![Coverage](badges/coverage.svg)
[![Docs](https://github.com/iahve-space/protolib/actions/workflows/docs.yml/badge.svg)](https://iahve-space.github.io/protolib/)
[![Conan package](https://img.shields.io/badge/conan-protolib-blue)](https://conan.io/center)
[![License](https://img.shields.io/github/license/iahve-space/protolib)](LICENSE)




---

## 🚀 Overview

`protolib` is a type-safe C++17 framework for binary protocol definition and manipulation.  
It helps describe message structures declaratively, ensuring **alignment correctness**, **endian awareness**, and **boundary safety** — while remaining portable between **Linux**, **macOS**, and embedded targets.

The project is part of the **iahve-space** infrastructure — a family of build-system-integrated tools (CMake + Conan) designed for reproducible, cross-platform embedded development.

---

## 💡 Motivation

This library was born out of practical necessity.
While working on embedded systems, I frequently faced the challenge of establishing a fast and reliable communication channel with an MPU \
— often under conditions where the protocol itself had to evolve dynamically.

This created a dual problem:
* I needed implementations for both PC and MCU environments,
* and at the same time, a unified way to verify that every protocol change preserved correctness and quality.

In traditional setups, protocol testing can’t easily be isolated into a standalone module. That leads to fragile designs and inconsistent behavior across platforms.
Over time, I experimented with multiple languages and approaches, but C++ proved to be the right choice — it offers powerful compile-time optimization tools like constexpr and templates, \
enabling high-performance, type-safe, and expressive designs.

The result is a solution that is:
*	Fully covered by tests,
*	Composable like a constructor — protocols are built from reusable fields,
*	and portable across Linux, embedded targets, and PC development groups.

---

## ⚙️ Design Philosophy

The core philosophy behind this project is clarity through composition.
Protocols are represented as structured sets of fields that can be easily defined, combined, and tested. Each field is strongly typed, self-describing, and compatible across platforms.

This modular design makes it possible to:
* Define communication layouts declaratively,
* Validate protocol integrity automatically,
* And reuse the same code across host, firmware, and simulation environments.

In essence, this library transforms protocol development into a testable, declarative, and scalable workflow for both embedded and high-level environments.


## ⚙️ Features

- 🧮 Declarative field definitions with automatic endian handling
- 🧱 Safe serialization and deserialization with bounds checking
- 🔄 TLV, fixed-layout, and variable-length record support
- 🧩 Template-based extensibility for custom protocols
- 🧰 Fully integrable with CMake + Conan build pipelines
- 🧪 Built-in unit tests (GoogleTest)
- 🔍 Static analysis (clang-tidy) and formatting (clang-format) integrated in CI

---

## 🧰 Build System Integration

### Requirements

- **CMake ≥ 3.24**
- **GCC ≥ 11 / Clang ≥ 14**
- (Optional) **Conan 2.x**

### CMake Configuration Variables

| Variable | Description |
|-----------|-------------|
| `SOFTWARE_VERSION` | Version string (passed from Conan or CI) |


---

## 📦 Installation

### 🧩 Option 1 – CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
  protolib
  GIT_REPOSITORY https://github.com/iahve-space/protolib.git
  GIT_TAG main
)
FetchContent_MakeAvailable(protolib)

target_link_libraries(your_app PRIVATE protolib)
```

Then you can use any libraries from source project or you can use common interface library 'protolib'

```cmake
target_link_libraries(protolib INTERFACE
        protolib_interfaces
        protolib_crc_soft
        protolib_crc16_modbus
        protolib_crc
        protolib_fields
        protolib_containers)
```

### 📦 Option 2 – Conan Package

```shell
conan create . --version="${VERSION}" -s:h build_type=Release
```

and then in your CMakeLists.txt:
```cmake
find_package(protolib REQUIRED)
target_link_libraries(your_app PRIVATE proto::protolib)
```
***(The project currently invokes Conan internally via execute_process(), but migration to cmake-conan helper is planned.)***

### 🧪 Testing

Unit tests use GoogleTest and are automatically discovered if BUILD_TESTING is enabled:

```shell
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
```
Tests are distributed across protocol definition modules, ensuring coverage for each binary structure.

### 📖 Documentation
1) API Reference (Doxygen).  [Pages](https://iahve-space.github.io/protolib/) 
2) Project Wiki – architecture, binary format details, and platform integration
3) Protocols directory – concrete protocol implementations with tests

⸻

### 💡 Quick Example

The library is designed as a protocol builder and separates reception logic from analysis logic. Furthermore, it's not tied to any specific architecture.\
So the example above demonstrates how easy it is to reuse a library class, for example, the exoatlantProtocolTest test template, by simply exposing it to buffer reception.

```c++
TEST(exoatlantProtocolTest, Main)
    {
        //debug включает подробный вывод парсера но отнимает значительное время на печать.
        ExoAtlantProtocol_<host_rx_buffer, host_tx_buffer> host_proto(false);

        Packet1 packet1_content{.number = 33, .data = {1,2,5,2,3,4}};
        auto packet = createPacket(packet1_content);

        uint result_size{};

        //добавлено для фрагментации данных. Так же можно отправлять фрагменты рандомных размеров
        {
            ScopeTimer scope("serialize");
            result_size = host_proto.serialize(packet,buf , sizeof(buf));
        }

        pkt_desc_t answer{};
        {
            ScopeTimer scope("deserialize");
            for (uint i = 0; i < result_size - 1; i++ )
            {
                host_proto.parse(buf+ i, 1);
            }
            answer = host_proto.parse(buf+ result_size - 1, 1);
        }
        ASSERT_EQ(packet, answer);
        host_proto.reset();
    }
```

Full list of implemented example protocols you can find in [protocols](./protocols) or dive in to [pages](https://iahve-space.github.io/protolib/) to learn more

### 🧭 Supported Platforms

| Platform | Compiler | Status |
|-----------|-----------|---------|
| Linux | GCC / Clang | ✅ |
| macOS | Clang | ✅ |
| Windows | MinGW / MSVC | ⚙️ |
| Embedded (NuttX / STM32) | Cross-compile via `BUILD_FOR_TARGET` | 🔧 |

> ⚠️ Some usage differences may apply on embedded targets — see the [Wiki](../../wiki) for details.

---

### 🧩 Continuous Integration

Current CI/CD stages:

| Stage | Purpose |
|--------|----------|
| **build** | Standard CMake build |
| **build-target** | Cross-build for embedded |
| **test** | Run unit tests |
| **coverage** | Collect and upload lcov/gcovr reports |
| **docs** | Generate and deploy Doxygen documentation |
| **analyzing** | Run clang-tidy and static analysis |
| **publish** | Build and upload Conan package |

### 🧑‍💻 Development Guidelines

* Source formatting: clang-format
* Static analysis: clang-tidy
* Unit tests: GoogleTest
* Lint, coverage, and analysis are enforced in CI

Local Build
```shell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target test
```
### 📜 License

Distributed under the GNU GPL v3 license.
See the [License](LICENSE) file for details.

### 🧭 Related Projects

All repositories under iahve-space share a unified CMake + Conan infrastructure
focused on embedded systems, cross-compilation, and reproducible builds.

© 2025 iahve-space. Built with ❤️ and C++17.
