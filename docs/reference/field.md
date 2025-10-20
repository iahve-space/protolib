# 🧩 FieldPrototype --- core building block

**FieldPrototype** is the fundamental abstraction representing a single
**field** within a protocol message.\
Each field directly manipulates memory within the protocol buffer --- no
heap allocations are ever performed.\
It ensures deterministic behavior, compile-time safety, and fine-grained
control of data layout.

-   Source:
    [FieldPrototype.hpp](https://github.com/iahve-space/protolib/blob/main/include/prototypes/field/FieldPrototype.hpp)\
-   Flags:
    [FieldFlags.hpp](https://github.com/iahve-space/protolib/blob/main/include/prototypes/field/FieldFlags.hpp)\
-   Enum:
    [FieldName.hpp](https://github.com/iahve-space/protolib/blob/main/include/prototypes/field/FieldName.hpp)\
-   Tests: [Tests
    directory](https://github.com/iahve-space/protolib/tree/main/include/prototypes/field/Tests)

------------------------------------------------------------------------

## ✨ Key Features

  -----------------------------------------------------------------------
Feature                        Description
  ------------------------------ ----------------------------------------
**Zero allocation**            Operates directly within the
pre-allocated protocol buffer.

**Strong typing**              Template-based type enforcement ensures
compile-time consistency.

**Per-field endianness**       `FieldFlags::REVERSE` enables byte-order
reversal only for specific fields.

**Dynamic sizing**             Use `kAnySize` and `SetSize()` to adapt
field size at runtime.

**Const fields**               `CONST_VALUE` enforces immutability for
compile-time constants.

**Matchers**                   Custom validation callbacks for
field-level checks.

**Debug print support**        Human-readable output for buffer dumps
and protocol tracing.
  -----------------------------------------------------------------------

------------------------------------------------------------------------

## ⚙️ Template Parameters

| Parameter      | Description |
|----------------|-------------|
| `NAME`         | Field identifier (`@ref FieldName`). |
| `T`            | The C++ type of the field (scalar, struct, or pointer). |
| `BASE`         | Pointer to the shared packet buffer. |
| `FLAGS`        | Combination of behavior flags (`@ref FieldFlags`). |
| `MAX_SIZE`     | Limit for dynamic fields. |
| `SIZE`         | Compile-time size (`kAnySize` = runtime). |
| `CONST_VALUE`  | Optional pointer to constant data. |
| `MATCHER`      | Optional callback for validation. |

------------------------------------------------------------------------

## 🧱 Internal Mechanics

Each `FieldPrototype` instance tracks:

-   **offset\_** --- byte offset within the shared buffer.\
-   **size\_** --- current runtime size.\
-   **read_count\_** --- number of read cycles.\
-   **flags\_** --- bitmask controlling field behavior.

All read/write operations are performed directly at `(BASE + offset_)`.

### Lifetime Stages

1.  **Construction** --- defines metadata statically within a
    container.\
2.  **Reset()** --- restores runtime parameters.\
3.  **ApplyConst()** --- copies constant data to buffer.\
4.  **Set()/GetCopy()** --- read/write access.\
5.  **Validation** --- optional match function.

------------------------------------------------------------------------

## 🔁 Endianness Control

The `FieldFlags::REVERSE` flag inverts byte order for **that field
only**, ensuring compatibility across MCU and PC environments.

``` cpp
using BE_U16 = FieldPrototype<
  FieldName::LEN_FIELD,
  uint16_t,
  base_ptr,
  FieldFlags::REVERSE | FieldFlags::IS_IN_CRC,
  2, 2
>;
```

------------------------------------------------------------------------

## 🧩 Protocol Composition

`FieldPrototype` instances form **Containers**, and containers form complete **Protocols**.  
**Important:** Every field can reference its **own base buffer**. You can bind all fields to the same contiguous buffer *or* point different fields to different buffers. The container tracks offsets and the **fill counter** per bound buffer and guarantees zero allocations.

### Typical layouts

**1) Single-buffer layout (simple case)**  
All fields map into one RX buffer:
```
RX buffer (buf_rx)
┌──────────────────────────────────────────────────────────────┐
│ [ID] [LEN] [TYPE] [DATA ...] [CRC]                           │
└──────────────────────────────────────────────────────────────┘
         ^     ^      ^        ^                ^
         |     |      |        |                |
       Field Field   Field    Field            Field
```

**2) Multi-buffer layout (advanced)**  
Some fields live in different buffers (e.g., payload arrives into a DMA ring):
```
RX header buffer (buf_hdr)                   RX payload buffer (buf_pay)
┌───────────────────────────────┐            ┌───────────────────────────┐
│ [ID] [LEN] [TYPE] [CRC] ...   │            │ [DATA ... possibly large] │
└───────────────────────────────┘            └───────────────────────────┘
   ^    ^     ^     ^                             ^
   |    |     |     |                             |
 Field Field Field Field                        Field
```

**3) Separate RX/TX buffers (common in practice)**  
RX and TX containers usually use different backing buffers:
```
+---------------- Protocol ----------------+
| RxContainer → binds to buf_rx            |
|   Fields: [ID][LEN][TYPE][DATA][CRC]     |
|                                          |
| TxContainer → binds to buf_tx            |
|   Fields: [ID][LEN][TYPE][DATA][CRC]     |
+------------------------------------------+
```

The same **container type** can be reused across different protocols or directions by **rebinding** its fields to another buffer set—no code duplication.


------------------------------------------------------------------------

## 🧪 Testing Example

``` cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
using ::testing::ElementsAre;

TEST(FieldPrototype, ReverseCopyWorks) {
  uint8_t buf[2] = {0};
  using F = FieldPrototype<FieldName::ID_FIELD, uint16_t, buf, FieldFlags::REVERSE>;
  F field;
  uint16_t val = 0x1234;
  field.Set(&val);
  EXPECT_THAT(buf, ElementsAre(0x34, 0x12));
}
```

------------------------------------------------------------------------

## 📚 See Also

-   [`FieldFlags`](https://github.com/iahve-space/protolib/blob/main/include/prototypes/field/FieldFlags.hpp)
-   [`FieldName`](https://github.com/iahve-space/protolib/blob/main/include/prototypes/field/FieldName.hpp)
-   [`Container`](https://github.com/iahve-space/protolib/tree/main/include/prototypes/container)
-   [`Protocol`](https://github.com/iahve-space/protolib/tree/main/include/prototypes/protocol)

------------------------------------------------------------------------
