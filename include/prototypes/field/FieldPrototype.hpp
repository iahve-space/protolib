#pragma once
/**
 * @file FieldPrototype.hpp
 * @brief FieldPrototype template for describing and handling protocol fields.
 *
 * Provides:
 *  - Definition of field metadata (name, flags, size, offset, const values);
 *  - Access to raw field data in a shared buffer;
 *  - Helpers for printing, setting, and resetting field content;
 *  - Traits for compile-time inspection of field properties;
 *  - Support for dynamic field sizes with an upper bound (@ref MAX_SIZE).
 */

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <vector>

#include "prototypes/field/FieldFlags.hpp"

namespace proto {

/**
 * @enum MatchStatus
 * @brief Status returned by matcher functions when comparing field values.
 */
enum class MatchStatus : uint8_t {
  NOT_MATCH,   //!< Field does not match expected value.
  PROCESSING,  //!< Matching is in progress.
  MATCH        //!< Field matches expected value.
};

/**
 * @struct EmptyDataType
 * @brief Marker type representing "no data".
 *
 * Used in templates where an optional data type is required.
 */
struct EmptyDataType {
  auto operator==(const EmptyDataType& other) const -> bool {
    (void)other;
    return true;
  }
};

/// Special constant meaning "size can be any".
static constexpr size_t K_ANY_SIZE = std::numeric_limits<size_t>::max();

/// Maximum allowed field size in bytes.
//    static constexpr size_t kMaxFieldSize = 2048;

/// Matcher callback type.
///
/// A matcher inspects the current field buffer and returns @ref MatchStatus.
/// Signature uses a raw pointer to the field storage; implementers should
/// cast to the appropriate type for read-only checks. (The pointer is not
/// owned and must not be stored.)
using MatcherType = MatchStatus (*)(void*);

/**
 * @class FieldPrototype
 * @brief Template for protocol field definition and runtime handling.
 *
 * Each field has:
 *  - Name (enum @ref proto::FieldName);
 *  - Type (`T`), which can be a plain type or pointer;
 *  - Shared buffer base pointer (`BASE`);
 *  - Flags (@ref FieldFlags);
 *  - Compile-time or dynamic size;
 *  - Optional constant value (for fixed fields).
 *
 * @tparam NAME_            FieldName identifier.
 * @tparam TYPE_            Field type (plain type or pointer).
 * @tparam BASE_         Base pointer to the shared buffer.
 * @tparam FLAGS_            FieldFlags mask.
 * @tparam MAX_SIZE     Hard cap for runtime size (safety limit for dynamic
 * fields).
 * @tparam SIZE         Compile-time size. Use @ref K_ANY_SIZE for dynamic-size
 * fields.
 * @tparam CONST_VALUE_  Optional pointer to a constant value copied on
 * reset/apply.
 * @tparam MATCHER      Optional matcher callback for validation/dispatch.
 */
template <
    FieldName NAME_, typename TYPE_, uint8_t* BASE_, FieldFlags FLAGS_,
    size_t MAX_SIZE = 4096,
    size_t SIZE = (std::is_pointer_v<TYPE_> ? K_ANY_SIZE : sizeof(TYPE_)),
    std::conditional_t<std::is_pointer_v<TYPE_>, std::remove_pointer_t<TYPE_>,
                       TYPE_>* CONST_VALUE_ = nullptr,
    MatcherType MATCHER = nullptr>
class FieldPrototype {
  template <typename Fields, typename TCrc>
  friend class FieldContainer;
  template <typename Fields, typename TCrc>
  friend class RxContainer;
  template <typename Fields, typename TCrc>
  friend class TxContainer;

 public:
  /// Resolved type (dereferenced if T is a pointer).
  using FieldType = std::conditional_t<std::is_pointer_v<TYPE_>,
                                       std::remove_pointer_t<TYPE_>, TYPE_>;

  /// Compile-time field metadata.
  static constexpr FieldName NAME{NAME_};
  static constexpr FieldFlags FLAGS{FLAGS_};
  static constexpr uint8_t* const BASE{BASE_};
  static constexpr FieldType* const CONST_VALUE{
      static_cast<FieldType*>(CONST_VALUE_)};
  //        static constexpr size_t max_size_{kMaxFieldSize};

  // Disable copying to ensure uniqueness.
  FieldPrototype(FieldPrototype&&) = delete;
  FieldPrototype(const FieldPrototype&) = delete;
  auto operator=(FieldPrototype&&) -> FieldPrototype& = delete;
  auto operator=(const FieldPrototype&) -> FieldPrototype& = delete;

  /**
   * @brief Construct a field with compile-time or dynamic size.
   *
   * Ensures that fixed-size fields match the provided SIZE.
   */
  FieldPrototype() {
    if constexpr (!std::is_pointer_v<TYPE_>) {
      if constexpr (sizeof(TYPE_) != SIZE) {
        static_assert(sizeof(TYPE_) == SIZE, "Size of T must match SIZE");
      }
    }
  }

  virtual ~FieldPrototype() = default;

  /// @return Pointer to the start of field data in the shared buffer (const).
  [[nodiscard]] auto begin() const -> const uint8_t* { return BASE + m_offset; }

  /// @return Pointer one past the end of field data in the shared buffer
  /// (const).
  [[nodiscard]] auto end() const -> const uint8_t* {
    return BASE + m_offset + m_size;
  }

  /// @return Current offset (relative to buffer start).
  [[nodiscard]] auto get_offset([[maybe_unused]] void* opt = nullptr) const
      -> size_t {
    return m_offset;
  }

  /// @return Current size in bytes (clamped to @ref MAX_SIZE when larger).
  [[nodiscard]] virtual auto get_size() const -> size_t {
    return m_size < MAX_SIZE ? m_size : MAX_SIZE;
  }

  /// @note Internal trait helpers to detect std::vector and derive a copy type.
  // ---- trait: это std::vector ? ----
  template <class>
  struct IsStdVector : std::false_type {};
  template <class U, class A>
  struct IsStdVector<std::vector<U, A>> : std::true_type {};

  template <class TT>
  static constexpr bool IS_STD_VECTOR_V = IsStdVector<TT>::value;

  using Elem = std::remove_pointer_t<TYPE_>;  // например, const unsigned char
  using ElemVal = std::remove_const_t<Elem>;  // unsigned char
  // ---- ваш тип возвращаемого значения ----
  // если поле-указатель (T = X*), вернуть std::vector<X>, иначе — T
  using CopyType =
      std::conditional_t<std::is_pointer_v<TYPE_>, std::vector<ElemVal>, TYPE_>;

  /**
   * @brief Return a value copy of the field content.
   *
   * If `T` is a pointer type, returns a contiguous `std::vector<ElemVal>`
   * sized from the current byte-length. If `T` is not a pointer, returns
   * a plain value of type `T` (using memcpy for trivially copyable types).
   */
  [[nodiscard]] auto get_copy() const -> CopyType {
    if constexpr (IS_STD_VECTOR_V<CopyType>) {
      // T — указатель ⇒ возвращаем вектор элементов
      const std::size_t FIELD_SIZE = get_size();  // сколько байт в поле
      const std::size_t COUNT = FIELD_SIZE / sizeof(Elem);  // сколько элементов
      CopyType val;  // это std::vector<Elem>
      val.resize(COUNT);
      if (COUNT != 0U) {
        // предполагаем, что данные плоские и совместимы по представлению
        std::memcpy(val.data(), get_ptr(), COUNT * sizeof(Elem));
      }
      return val;
    } else {
      // T — не указатель ⇒ вернуть T по значению
      CopyType value{};
      if constexpr (std::is_trivially_copyable_v<CopyType>) {
        // безопасно побайтно
        std::memcpy(&value, get_ptr(), sizeof(CopyType));
      } else {
        // Лучше явно сконструировать из байтов, если есть такой конструктор,
        // или написать парсер; memcpy для нетривиальных типов — UB.
        // Пример (если у вас есть подходящий конструктор):
        // value = CopyType(reinterpret_cast<const std::byte*>(get_ptr()),
        // GetSize());
        static_assert(std::is_trivially_copyable_v<CopyType>,
                      "Provide a parser/constructor for non-trivial CopyType");
      }
      return value;
    }
  }
  /// @return Typed pointer to the field storage (read-only view).
  [[nodiscard]] constexpr auto get_ptr() const -> const FieldType* {
    return reinterpret_cast<const FieldType*>(BASE + m_offset);
  }

  /**
   * @brief Print field contents in a table format.
   *
   * Shows:
   *  - Field name
   *  - Value (hex)
   *  - Size
   *  - Offset
   *  - Inclusion in length / CRC as defined by @ref FieldFlags::IS_IN_LEN and
   * @ref FieldFlags::IS_IN_CRC
   *  - Const value (if defined)
   */
  void print() {
    std::cout << '\n' << std::string(90, '-') << '\n';
    std::cout << "| " << std::setw(15) << std::left << "FieldName"
              << " | " << std::setw(24) << "Value (Hex)"
              << " | " << std::setw(6) << "Size"
              << " | " << std::setw(6) << "Offset"
              << " | " << std::setw(10) << "Is in len"
              << " | " << std::setw(10) << "Is in crc" << " |\n";

    using Type = std::decay_t<decltype(*this)>;

    std::ostringstream hex_stream;
    const auto* data = reinterpret_cast<const uint8_t*>(begin());
    for (size_t i = 0; i < get_size(); ++i) {
      hex_stream << std::hex << std::uppercase << std::setw(2)
                 << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }

    std::cout << "| " << std::setw(15) << to_string(Type::NAME) << " | "
              << std::setw(24) << hex_stream.str() << " | " << std::setw(6)
              << get_size() << " | " << std::setw(6) << get_offset() << " | "
              << std::setw(10)
              << (((FLAGS & FieldFlags::IS_IN_LEN) != FieldFlags::NOTHING)
                      ? "TRUE"
                      : "FALSE")
              << " | " << std::setw(10)
              << (((FLAGS & FieldFlags::IS_IN_CRC) != FieldFlags::NOTHING)
                      ? "TRUE"
                      : "FALSE")
              << " |";

    if (CONST_VALUE != nullptr) {
      std::ostringstream const_stream;
      for (size_t i = 0; i < get_size(); ++i) {
        const_stream << std::hex << std::uppercase << std::setw(2)
                     << std::setfill('0')
                     << static_cast<int>(
                            reinterpret_cast<const uint8_t*>(CONST_VALUE)[i])
                     << " ";
      }
      std::cout << "\n| " << std::setw(15) << "ConstValue"
                << " | " << std::setw(24) << const_stream.str() << " |";
    }
  }

 protected:
  /// Runtime state: how many bytes were read into this field.
  size_t m_read_count{};
  /// Optional matcher callback for this field.
  MatcherType m_matcher = MATCHER;
  /// Current size (may differ from @ref SIZE if dynamic).
  size_t m_size{SIZE};

  /// Offset in the shared buffer.
  size_t m_offset{0};

  /**
   * @brief Configure runtime size for dynamic fields.
   *
   * Effective only when `SIZE == K_ANY_SIZE`. The new size is taken from
   * `size_to_set` when provided, otherwise deduced from `sizeof(DATA)`
   * when `DATA` is not a pointer and not @ref proto::EmptyDataType.
   * The value is clamped by @ref MAX_SIZE. For fields with CONST_VALUE
   * the size cannot be changed.
   *
   * @tparam DATA      Element type used for deduction when size is not
   * provided.
   * @param  size_to_set Byte length to set, or @ref K_ANY_SIZE to auto-deduce.
   * @return true if size accepted (or unchanged), false if it exceeds @ref
   * MAX_SIZE.
   */
  template <class DATA>
  constexpr auto set_size(size_t size_to_set = K_ANY_SIZE) -> bool {
    if constexpr (CONST_VALUE != nullptr) {
      static_assert(CONST_VALUE == nullptr,
                    "You can't set size for fields with const_value");
    }
    if constexpr (SIZE == K_ANY_SIZE) {
      if constexpr (!std::is_same_v<std::remove_pointer_t<DATA>,
                                    proto::EmptyDataType>) {
        constexpr size_t K_STATIC_SIZE =
            std::is_pointer_v<DATA> ? 0 : sizeof(DATA);
        if (size_to_set != K_ANY_SIZE) {
          if (size_to_set <= MAX_SIZE) {
            this->m_size = size_to_set;
          } else {
            return false;
          }
        } else if constexpr (K_STATIC_SIZE > 0) {
          this->m_size = K_STATIC_SIZE;
        }
      } else {
        this->m_size = 0;
      }
    }
    return true;
  }

  /// Assign byte offset relative to the shared buffer start.
  void set_offset(size_t offset) { m_offset = offset; }

  /// Set field value from a typed object; forwards to the void* overload.
  constexpr void set(const TYPE_& value) {
    static_assert(CONST_VALUE == nullptr,
                  "Const value mustn't be provided for Set");
    if constexpr (std::is_pointer_v<TYPE_>) {
      set(static_cast<const void*>(value));
    } else {
      set(static_cast<const void*>(&value));
    }
  }

  /**
   * @brief Set field value from raw bytes.
   *
   * Copies `GetSize()` bytes into the field storage. When @ref
   * FieldFlags::REVERSE is set, bytes are written in reverse order.
   */
  virtual void set(const void* value) {
    if constexpr ((FLAGS & FieldFlags::REVERSE) != FieldFlags::NOTHING) {
      const auto* src = static_cast<const uint8_t*>(value);
      for (size_t i = 0; i < get_size(); ++i) {
        (BASE + m_offset)[i] = src[get_size() - 1 - i];
      }
    } else {
      std::memcpy(BASE + m_offset, value, get_size());
    }
  }

  /**
   * @brief Copy @ref CONST_VALUE into the storage (if defined), honoring @ref
   * FieldFlags::REVERSE.
   */
  void apply_const() const {
    if constexpr (CONST_VALUE != nullptr) {
      if constexpr ((FLAGS & FieldFlags::REVERSE) != FieldFlags::NOTHING) {
        const auto* src = reinterpret_cast<const uint8_t*>(CONST_VALUE);
        for (size_t i = 0; i < get_size(); ++i) {
          (BASE + m_offset)[i] = src[get_size() - 1 - i];
        }
      } else {
        std::memcpy(BASE + m_offset, CONST_VALUE, get_size());
      }
    }
  }

  /// Reset runtime state (offset/read_count/size) to defaults.
  virtual void reset() {
    m_offset = 0;
    m_read_count = 0;
    m_size = SIZE;
  }
};

/**
 * @struct FieldTraits
 * @brief Compile-time helper to extract field properties.
 *
 * Provides compile-time mirrors of the field's static metadata so it can be
 * used in type-metaprogramming contexts without an instance.
 */
template <typename Field>
struct FieldTraits {
  using Type = std::remove_reference_t<Field>;
  static constexpr auto CONST_VALUE = Type::CONST_VALUE;
  static constexpr auto NAME = Type::NAME;
  static constexpr auto FLAGS = Type::FLAGS;
  static constexpr bool IS_CONST = (Type::CONST_VALUE != nullptr);
};

/**
 * @struct fieldsTuple
 * @brief Helper to unwrap a FieldPrototype into a `Type` alias used by
 * container tuples.
 */
template <typename Field>
struct fieldsTuple;  // base template

template <FieldName NAME, typename T, uint8_t* BASE, FieldFlags FLAGS,
          size_t MAX_SIZE, size_t SIZE, auto* CONST_VALUE, MatcherType MATCHER>
struct fieldsTuple<proto::FieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE,
                                         CONST_VALUE, MATCHER>> {
  using Type = FieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE, CONST_VALUE,
                              MATCHER>;
};

}  // namespace proto