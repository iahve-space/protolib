#pragma once
#pragma once
/**
 * @file FieldContainer.hpp
 * @brief Defines the FieldContainer class template, a generic container for
 * protocol fields.
 *
 * This header provides a generic container template that holds protocol field
 * instances, manages CRC calculation, supports debug facilities, and provides
 * convenient access and iteration over the fields by name or index. The
 * container is designed to work with protocol field types, enabling lookup by
 * name or index, state reset, and debug output for protocol development and
 * diagnostics.
 */

#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "libraries/crc/crcSoft/CrcSoft.hpp"
#include "prototypes/field/DataField.hpp"
#include "prototypes/field/FieldPrototype.hpp"

namespace proto {
/**
 * @brief Transform a tuple of field prototypes into a tuple of actual field
 * types.
 *
 * This struct template is used to convert a tuple of field prototypes
 * (such as FieldPrototype types) into a tuple of the actual field types
 * to be stored in the container.
 *
 * @tparam Tuple Tuple of field prototype types.
 */
template <typename Tuple>
struct TransformToFieldsTuple;

/**
 * @brief Specialization of TransformToFieldsTuple for std::tuple of Fields.
 *
 * For a tuple of field prototypes, produces a tuple of the corresponding field
 * types. Used internally by FieldContainer to instantiate storage for all
 * fields.
 *
 * @tparam Fields Variadic list of field prototype types.
 */
template <typename... Fields>
struct TransformToFieldsTuple<std::tuple<Fields...>> {
  using Type = std::tuple<typename fieldsTuple<Fields>::Type...>;
};

/**
 * @brief Metafunction to find a field type by its FieldName within a tuple of
 * field types.
 *
 * @tparam Tuple  Tuple of concrete field types (not prototypes).
 * @tparam NAME   Target FieldName to search for.
 * @tparam Index  Current index (implementation detail).
 */
// Lookup helper that avoids out-of-bounds tuple_element instantiation
template <typename Tuple, FieldName NAME, std::size_t Index,
          bool AtEnd = (Index >= std::tuple_size_v<Tuple>)>
struct FieldByNameImpl;

// End of recursion: no such field
template <typename Tuple, FieldName NAME, std::size_t Index>
struct FieldByNameImpl<Tuple, NAME, Index, true> {
  using type = void;
};

// Recursive case
template <typename Tuple, FieldName NAME, std::size_t Index>
struct FieldByNameImpl<Tuple, NAME, Index, false> {
  using Curr = std::tuple_element_t<Index, Tuple>;
  using type = std::conditional_t<
      (Curr::NAME == NAME), Curr,
      typename FieldByNameImpl<Tuple, NAME, Index + 1>::type>;
};

// Public alias: preserves the existing FieldByName<...>::type usage
template <typename Tuple, FieldName NAME, std::size_t Index = 0>
using FieldByName = FieldByNameImpl<Tuple, NAME, Index>;

/**
 * @brief Generic container for protocol fields with CRC and debug support.
 *
 * This class template stores a tuple of protocol field instances, provides
 * methods for accessing fields by name or index, resetting all fields and CRC
 * state, and iterating over all fields. It also supports debug output for
 * protocol diagnostics.
 *
 * @tparam Fields Tuple of field prototype types, which will be instantiated as
 * fields.
 * @tparam TCrc CRC calculation class to use (default: CrcSoft).
 *
 * Usage:
 *   - Use Get<FieldName>() or Get<Index>() to access fields by name or index.
 *   - Use HasField<FieldName>() to check for field presence.
 *   - Use Reset() to reset all fields and CRC to default state.
 *   - Use for_each_type() to apply a function to each field.
 *   - Set debug mode via SetDebug().
 */
template <typename Fields, typename TCrc = CrcSoft>
class FieldContainer {
 public:
  virtual ~FieldContainer() = default;
  /**
   * @brief Construct a FieldContainer.
   *
   * Default constructor initializes all fields and CRC to default state.
   */
  FieldContainer() = default;

  /**
   * @brief Tuple type with concrete field instances for this container.
   */
  using FieldsTuple = typename TransformToFieldsTuple<Fields>::Type;

  /**
   * @brief Return type deduced from DATA_FIELD at compile time.
   *
   * - If the container has a DATA_FIELD and that field satisfies
   *   `is_data_field_prototype<>::value`, this resolves to the field's
   * `Variant` type.
   * - If the container has a DATA_FIELD that is a regular field, this resolves
   * to the field's `FieldType`.
   * - If the container has no DATA_FIELD, this resolves to `void`.
   */

  using DataField =
      typename FieldByName<FieldsTuple, FieldName::DATA_FIELD>::type;

  using ReturnType = std::remove_const_t<std::remove_reference_t<
      decltype(std::declval<const DataField&>().get_copy())>>;

  /**
   * @brief Enable or disable debug mode for the container and its fields.
   *
   * When debug is enabled, additional diagnostic information may be output
   * during protocol processing.
   *
   * @param DEBUG Set true to enable debug mode, false to disable.
   */
  void set_debug(const bool DEBUG) { m_debug = DEBUG; }

  /**
   * @brief Check if debug mode is enabled.
   *
   * @return true if debug mode is enabled, false otherwise.
   */
  [[nodiscard]] auto is_debug() const -> bool { return m_debug; }

  /**
   * @brief The number of fields in the container.
   */
  static constexpr std::size_t SIZE = std::tuple_size_v<FieldsTuple>;

  /**
   * @brief Access a field by its FieldName (compile-time constant).
   *
   * Usage: container.Get<MY_FIELD_NAME>()
   *
   * @tparam NAME The FieldName enum or identifier of the field.
   * @tparam Index (internal, do not specify) Index to start searching from.
   * @return Reference to the field with the given name.
   * @throws (compile-time) If the field is not found, returns the first field
   * (may static_assert or throw in future).
   */
  template <FieldName NAME, std::size_t Index = 0>
  constexpr auto get() -> auto& {
    if constexpr (Index >= std::tuple_size_v<decltype(m_fields)>) {
      return std::get<0>(m_fields);  // or throw an exception
    } else if constexpr (std::tuple_element_t<Index,
                                              decltype(m_fields)>::NAME ==
                         NAME) {
      return std::get<Index>(m_fields);
    } else {
      return get<NAME, Index + 1>();
    }
  }

  /**
   * @brief Access a field by its zero-based index.
   *
   * Usage: container.Get<0>() for the first field, etc.
   *
   * @tparam INDEX Index of the field (compile-time constant).
   * @return Reference to the field at the given index.
   * @throws (compile-time) If index is out of bounds, triggers a static_assert.
   */
  template <uint64_t INDEX>
  constexpr auto get() -> auto& {
    return std::get<INDEX>(m_fields);
  }

  /**
   * @brief Check if a field with the given FieldName exists in the container.
   *
   * Usage: FieldContainer::HasField<MY_FIELD_NAME>()
   *
   * @tparam NAME The FieldName enum or identifier of the field.
   * @tparam Index (internal, do not specify) Index to start searching from.
   * @return true if the field exists, false otherwise.
   */
  template <FieldName NAME, std::size_t Index = 0>
  static constexpr auto has_field() -> bool {
    if constexpr (Index >= std::tuple_size_v<FieldsTuple>) {
      return false;
    } else if constexpr (std::tuple_element_t<Index, FieldsTuple>::NAME ==
                         NAME) {
      return true;
    } else {
      return has_field<NAME, Index + 1>();
    }
  }

  /**
   * @brief Reset all fields, CRC, and internal state to defaults.
   *
   * This method resets the CRC calculator, all field values (by calling their
   * Reset()), clears all field offsets, and resets the field index.
   *
   * Usage: Call before filling the container with new data.
   */
  virtual void reset() {
    this->m_crc.reset();
    this->for_each_type([&](auto& field) { field.reset(); });
    for (auto& [key, value] : m_offsets) {
      value = 0;
    }
    m_field_index = 0;
  }

  /**
   * @brief Apply a callable to each field in the container.
   *
   * The callable should accept a reference to the field.
   * Usage: container.for_each_type([](auto& field){ ... });
   *
   * @tparam Func Type of the callable.
   * @param FUNC Callable to apply to each field.
   */
  template <typename Func>
  constexpr void for_each_type(Func&& FUNC) {
    constexpr std::size_t TUPLE_SIZE = std::tuple_size_v<FieldsTuple>;
    for_each_type_impl(this->m_fields, std::forward<Func>(FUNC),
                       std::make_index_sequence<TUPLE_SIZE>{});
  }

  // --- Build a tuple of named types deduced the same way as ReturnType
  // Each element keeps the field's compile-time name and the type
  // produced by Field::get_copy() for that concrete field.

  // Helper: deduce raw copy type and normalize it
  template <class Field>
  using _field_copy_raw_t = std::remove_const_t<std::remove_reference_t<
      decltype(std::declval<const Field&>().get_copy())>>;

  // If get_copy returns pointer -> turn into std::vector<non-const T>
  template <class T>
  struct PtrToVec {
    using type = T;
  };
  template <class U>
  struct PtrToVec<U*> {
    using type = std::vector<std::remove_const_t<U>>;
  };
  template <class U>
  struct PtrToVec<const U*> {
    using type = std::vector<std::remove_const_t<U>>;
  };

  // If get_copy returns std::vector<const T> -> make it std::vector<T>
  template <class T>
  struct StripConstFromVector {
    using type = T;
  };
  template <class U, class Alloc>
  struct StripConstFromVector<std::vector<U, Alloc>> {
    using type = std::vector<std::remove_const_t<U>>;
  };

  template <class Field>
  using _field_copy_t = typename StripConstFromVector<
      typename PtrToVec<_field_copy_raw_t<Field>>::type>::type;

  // Named holder: stores compile-time field name and value type
  template <FieldName NAME_, class T_>
  struct NamedCopyType {
    static constexpr FieldName NAME = NAME_;
    using value_type = T_;
  };

  // Make a tuple of named copy types for all fields in FieldsTuple
  template <typename Tuple_>
  struct MakeReturnTuple;

  template <typename... Field>
  struct MakeReturnTuple<std::tuple<Field...>> {
    using Type =
        std::tuple<NamedCopyType<Field::NAME, _field_copy_t<Field>>...>;
  };

  // Public alias: tuple of elements with (NAME, value_type) matching get_copy()
  // of each field
  using ReturnValuesTuple = typename MakeReturnTuple<FieldsTuple>::Type;

  // --- Runtime return tuples with actual copied values ---
  // 1) Tuple of pure value types (without name wrappers)
  template <typename Tuple>
  struct MakeValueTuple;

  template <typename... Field>
  struct MakeValueTuple<std::tuple<Field...>> {
    using Type = std::tuple<_field_copy_t<Field>...>;
  };

  using ReturnTuple = typename MakeValueTuple<FieldsTuple>::Type;

  // 2) Named value element that carries compile-time name and a runtime value
  template <FieldName NAME_, class T_>
  struct NamedValue {
    static constexpr FieldName NAME = NAME_;
    T_ m_value;  // runtime copy
  };

  // 3) Tuple of named values (compile-time name + runtime value)
  template <typename Tuple>
  struct MakeNamedValueTuple;

  template <typename... Field>
  struct MakeNamedValueTuple<std::tuple<Field...>> {
    using Type = std::tuple<NamedValue<Field::NAME, _field_copy_t<Field>>...>;
  };

  // --- helpers for GetCopies / GetNamedCopies ---
  template <std::size_t... Is>
  [[nodiscard]] auto get_copies_impl(
      std::index_sequence<Is...> /*unused*/) const -> ReturnTuple {
    const auto& tup = this->m_fields;
    return ReturnTuple{(normalize_value(std::get<Is>(tup).get_copy()))...};
  }

  using NamedReturnTuple = typename MakeNamedValueTuple<FieldsTuple>::Type;

  // Copy every field and return a tuple of raw values
  [[nodiscard]] auto get_copies() const -> ReturnTuple {
    return GetCopiesImpl(
        std::make_index_sequence<std::tuple_size_v<FieldsTuple>>{});
  }

  // Copy every field and return a tuple of named values
  [[nodiscard]] auto get_named_copies() const -> NamedReturnTuple {
    return get_named_copies_impl(
        std::make_index_sequence<std::tuple_size_v<FieldsTuple>>{});
  }

  template <std::size_t... Is>
  [[nodiscard]] auto get_named_copies_impl(
      std::index_sequence<Is...> /*unused*/) const -> NamedReturnTuple {
    const auto& tup = this->m_fields;
    return NamedReturnTuple{
        NamedValue<std::tuple_element_t<Is, FieldsTuple>::NAME,
                   _field_copy_t<std::tuple_element_t<Is, FieldsTuple>>>{
            normalize_value(std::get<Is>(tup).get_copy())}...};
  }

 protected:
  /**
   * @brief Debug flag for enabling/disabling protocol debug output.
   */
  bool m_debug = false;
  /**
   * @brief Tuple holding all field instances.
   */
  FieldsTuple m_fields;
  /**
   * @brief Map of field buffer pointers to their offsets (used for
   * serialization/deserialization).
   */
  std::unordered_map<uint8_t*, size_t> m_offsets;
  /**
   * @brief CRC calculator instance for the protocol frame.
   */
  TCrc m_crc{};
  /**
   * @brief Current field index for iteration or state tracking.
   */
  size_t m_field_index{};

  /**
   * @brief Helper to apply a callable to each field in the tuple
   * (implementation).
   *
   * @tparam Tuple The tuple type holding the fields.
   * @tparam Func The callable to apply.
   * @tparam Is Index sequence for tuple expansion.
   * @param obj Tuple of fields.
   * @param FUNC Callable to apply.
   */
  template <typename Tuple, typename Func, std::size_t... Is>
  constexpr void for_each_type_impl(Tuple& obj, Func&& FUNC,
                                    std::index_sequence<Is...> /*unused*/) {
    (FUNC.template operator()<std::tuple_element_t<Is, Tuple>>(
         std::get<Is>(obj)),
     ...);
  }

 private:
  /**
   * @brief Number of fields in the container (internal constant).
   */
  static constexpr std::size_t COUNT = std::tuple_size_v<decltype(m_fields)>;

  // value normalizer for runtime conversion
  template <class V>
  static auto normalize_value(V&& VAL) {
    return std::forward<V>(VAL);
  }
  template <class U>
  static auto normalize_value(const std::vector<const U>& VAL) {
    return std::vector<std::remove_const_t<U>>(VAL.begin(), VAL.end());
  }
  template <class U>
  static auto normalize_value(std::vector<const U>&& VAL) {
    return std::vector<std::remove_const_t<U>>(VAL.begin(), VAL.end());
  }
};

}  // namespace proto