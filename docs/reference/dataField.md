# DataField

> **Inherits:** \ref FieldPrototype (base field contract)
>
> **See also:** \ref FieldName, \ref ContainerPrototype

`DataField` (aka **`DataFieldPrototype`** in code) is a flexible protocol field that provides a *typed* view over a byte buffer without dynamic allocations. It works with a compile‑time buffer base (`BASE`) and a runtime `offset` supplied by the container.

---

## Goals
- **Zero‑copy access**: read/write directly into the protocol buffer segment.
- **Per‑field endianness**: each field can specify its own Little/Big‑Endian policy.
- **Static shape**: element type and field footprint are known at compile time.
- **Variant payloads**: represent data as a tagged union of alternatives when schema requires it.
- **Safety by construction**: strong types, bounds checks in debug, no accidental reallocations.

---

## Memory model
`DataField` never owns memory and never re-binds. Its address base is a **compile‑time** pointer supplied via the `BASE` template parameter, and it adds the field's runtime/computed `offset` to it. This keeps pointer arithmetic trivial and lets the compiler inline aggressively.

**Important lifetimes**
- Pointers/spans returned by `DataField` are valid as long as the underlying buffer at `BASE` remains alive and unmoved.
- Offsets may be recomputed by the container/layout logic; if your layout changes, re‑query pointers/spans.

---

## Endianness
Each `DataField` applies the protocol's per‑field endianness policy (configured alongside the field definition). Bytes are converted on read/write; the raw bytes in the buffer stay in the canonical wire order you define.

---

## Template parameters

| Parameter | Meaning |
|---|---|
| `PACKETS` | `std::tuple` of `PacketInfo` entries describing the set of payload alternatives (tags, sizes, and interpretation rules). |
| `BASE`    | Non‑type template parameter – pointer to the start of the shared buffer (`uint8_t*`). The field addresses memory as `BASE + offset`. |
| `FLAGS`   | `FieldFlags` bitmask controlling participation in `LEN`/`CRC`, debug behavior, etc. |
| `MAX_SIZE`| Maximum byte size for pointer/variable‑length payloads (compile‑time cap). Defaults to `4096`. |

---

## API sketch

> Exact names can vary slightly across headers; consult the code for definitive signatures.

- **Metadata**
  - `size_bytes()`, `offset()`, `is_in_len()`, `is_in_crc()` — structural markers used by the container.
- **Raw views**
  - `auto* get_ptr() noexcept` — pointer to the beginning of the field inside the buffer.
  - `std::span<std::byte>` / `std::span<const std::byte>` — contiguous view (for byte payloads).
- **Typed access**
  - `get()` — returns a value (or variant) for the field, endian‑aware.
  - `set(...)` — writes a value or variant into the buffer with proper endianness.
  - For composite payloads a `std::variant<...>` alternative may be returned; see **Variant payloads** below.
- **Debug**
  - `print_debug(std::ostream&)` — hex‑dump with size/offset and participation flags (used by the protocol trace you see in tests).

---

## Variant payloads
Some protocols multiplex different shapes behind a single *type* code. `DataField` supports this via a `std::variant`‑like representation:

- On **read**, the field inspects the context (e.g., a sibling `TYPE_FIELD`) and returns the active alternative — either as a copy of `T` or a lightweight view into the buffer when the alternative is an array/bytes.
- On **write**, you set the active alternative; the field encodes it in place.

> When returning a scalar through a variant, the implementation may construct a temporary `T` value. For zero‑copy needs prefer `get_ptr()` / span‑based views where applicable.

---

## Usage examples

> These are illustrative; concrete aliases for your project likely live near field/type definitions and provide `PACKETS`, `FLAGS`, and `BASE`.

### Fixed‑size scalar (little‑endian on wire)
```cpp
// Assume: using Reg16 = DataField<RegPackets, RX_BASE, FieldFlags::InLen|FieldFlags::InCrc, 2>;
Reg16 reg16;                // offset is assigned by the container
// write value into buffer at BASE + offset with LE encoding
reg16.set(static_cast<std::uint16_t>(0xABCD));
// read it back as a value (host endianness)
auto v = reg16.get();
```

### Zero‑copy byte region
```cpp
// Assume: using Payload = DataField<ByteArrayPackets, RX_BASE, FieldFlags::InLen|FieldFlags::InCrc, 32>;
Payload data;
auto* p = data.get_ptr();                 // raw pointer into the shared buffer
std::span<std::byte> bytes{p, 32};       // zero‑copy view for bulk ops
```

### Variant driven by a TYPE field
```cpp
// Assume PACKETS maps a type tag to alternatives like std::uint32_t or MyStruct3
using Flex = DataField<AltPackets, RX_BASE, FieldFlags::InLen|FieldFlags::InCrc, 8>;
Flex flexible;
std::variant<std::uint32_t, MyStruct3> val = flexible.get();
```

---

## Integration with the container
- The \ref ContainerPrototype computes `offset` for each field and, depending on your layout, may map different fields to **different buffers** by instantiating them with distinct `BASE` pointers. RX and TX commonly use separate buffers.
- Flags `is_in_len` / `is_in_crc` let the container compute length/CRC over *selected* fields only (common with wire protocols).
- RX/TX may use **separate buffers**; fields are free to map to different physical regions as your design dictates.

---

## Debug tracing
The project ships a structured hex table (see test logs) produced by `print_debug()`. It prints **Name**, **Value (hex)**, **Size**, **Offset**, and whether the bytes participate in **LEN**/**CRC**. This is invaluable for protocol bring‑up and mismatch hunting.

---

## Performance notes
- All hot paths are constexpr‑/template‑friendly and avoid heap work.
- Endian swaps use cheap bit/byte operations (optimized away for native order).
- Favor `get_ptr()`/span for bulk payload handling to stay zero‑copy.

---

## Pitfalls & tips
- Ensure the memory behind `BASE` outlives any pointer/span obtained from a field.
- If your field’s size depends on another field (e.g., `LEN_FIELD`), ensure the dependency is declared so the container re‑computes layout in the right order.
- For CRC/Length windows, mark only the intended fields with the `is_in_crc`/`is_in_len` traits.

---

## Code
- Base contract: [`FieldPrototype`](../include/prototypes/field/FieldPrototype.hpp)
- Data field implementation: [`DataFieldPrototype`](../include/prototypes/field/DataFieldPrototype.hpp)
- Example usages in tests: [`fieldTests`](../include/prototypes/field/Tests)

> The exact relative paths above assume the default repository layout used by this project.
