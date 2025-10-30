#pragma once
/**
 * @file FieldInfo.hpp
 * @brief Helpers and utilities for compile-time field metadata and tuple
 * processing.
 *
 * This header provides:
 *  - @ref proto::FieldInfo — a lightweight metadata wrapper for a field pointer
 * and its size;
 *  - @ref proto::MakeFieldInfo — a helper to construct FieldInfo with
 * deduction;
 *  - @ref proto::FieldInfoHasName — compile-time check for presence of a field
 * by name in a tuple;
 *  - @ref proto::GetFieldInfoByName — compile-time retrieval of a field info by
 * name from a tuple;
 *  - @ref proto::ForEachInfo — compile-time for-each over a tuple;
 *  - @ref proto::AppendToInfo — helper to append a new element to a tuple at
 * compile time.
 */

#include <tuple>
#include <type_traits>
#include <utility>

#include "prototypes/field/FieldPrototype.hpp"

namespace proto {

/**
 * @brief Metadata wrapper for a field pointer and its size, tagged by a
 * FieldName.
 *
 * @tparam N FieldName tag identifying the field.
 * @tparam T    Field data type.
 *
 * @note This struct does **not** own the memory pointed to by @ref data.
 */
template <FieldName N, typename T>
struct FieldInfo {
  /// Raw pointer to field data (non-owning).
  T* m_data;

  /// Exposed type alias for the data type.
  using Type = T;

  /**
   * @brief Construct FieldInfo from pointer and size.
   * @param data  Pointer to the data buffer.
   * @param size  Size of the data in bytes (defaults to sizeof(T) in the
   * overload below).
   */
  explicit FieldInfo(T* data, size_t size) : m_data(data), m_size(size) {}

  /// Compile-time field name tag.
  static constexpr FieldName NAME{N};

  /// Size of the data in bytes.
  size_t m_size{sizeof(T)};
};

/**
 * @brief Helper to create a FieldInfo with type/size deduction.
 *
 * @tparam NAME FieldName tag.
 * @tparam T    Field data type.
 * @param ptr   Pointer to the data.
 * @param size  Size in bytes (defaults to sizeof(T)).
 * @return A FieldInfo instance.
 */
template <FieldName NAME, typename T>
constexpr auto make_field_info(T* ptr, size_t size = sizeof(T)) {
  return FieldInfo<NAME, T>{ptr, size};
}

/**
 * @brief Compile-time check whether a tuple of FieldInfo-like elements contains
 * a field with the given name.
 *
 * @tparam TargetName Name to look for.
 * @tparam Tuple      Tuple type (e.g., std::tuple<...>).
 * @tparam Index      Internal recursion index (do not pass).
 * @return true if a matching element is found; false otherwise.
 */
template <FieldName TargetName, typename Tuple, size_t Index = 0>
static constexpr auto field_info_has_name() -> bool {
  using TupleType = std::remove_reference_t<Tuple>;
  if constexpr (Index >= std::tuple_size_v<TupleType>) {
    return false;
  } else {
    using CandidateT = std::tuple_element_t<Index, TupleType>;
    if constexpr (CandidateT::NAME == TargetName) {
      return true;
    } else {
      return field_info_has_name<TargetName, Tuple, Index + 1>();
    }
  }
}

/**
 * @brief Retrieve a FieldInfo-like element from a tuple by its @ref FieldName.
 *
 * Callers should ensure at compile time that the tuple indeed contains the
 * name, for example by using @ref FieldInfoHasName before calling this
 * function.
 *
 * @tparam NAME  FieldName to retrieve.
 * @tparam Tuple Tuple type.
 * @tparam Index Internal recursion index (do not pass).
 * @param tuple  Tuple instance.
 * @return Reference to the matching element inside the tuple.
 *
 * @warning If no element is found, the function reaches @c
 * __builtin_unreachable(). Use @ref FieldInfoHasName to guard calls in
 * constant-evaluated contexts.
 */
template <FieldName NAME, typename Tuple, std::size_t Index = 0>
constexpr auto get_field_info_by_name(Tuple&& tuple) -> decltype(auto) {
  using TupleType = std::remove_reference_t<Tuple>;

  if constexpr (Index < std::tuple_size_v<TupleType>) {
    auto&& candidate = std::get<Index>(tuple);
    if constexpr (std::remove_reference_t<decltype(candidate)>::NAME == NAME) {
      return candidate;
    } else {
      return get_field_info_by_name<NAME, Tuple, Index + 1>(
          std::forward<Tuple>(tuple));
    }
  } else {
    // We rely on prior compile-time checks (FieldInfoHasName) to ensure this
    // path is never taken.
    __builtin_unreachable();
  }
}

/**
 * @brief Implementation detail for @ref ForEachInfo: fold over tuple indices.
 *
 * @tparam Tuple Tuple type.
 * @tparam Func  Callable taking tuple elements.
 * @tparam Is    Index sequence.
 * @param tuple  Tuple instance.
 * @param FUNC      Callable to apply to each element.
 */
template <typename Tuple, typename Func, std::size_t... Is>
constexpr void for_each_impl(Tuple&& tuple, Func&& FUNC,
                             std::index_sequence<Is...> /*unused*/) {
  (FUNC(std::get<Is>(std::forward<Tuple>(tuple))), ...);
}

/**
 * @brief Apply a callable to each element of a tuple.
 *
 * @tparam Tuple Tuple type.
 * @tparam Func  Callable type; must be invocable with each tuple element.
 * @param tuple  Tuple instance.
 * @param FUNC      Callable to apply.
 */
template <typename Tuple, typename Func>
constexpr void for_each_info(Tuple&& tuple, Func&& FUNC) {
  constexpr std::size_t SIZE =
      std::tuple_size_v<std::remove_reference_t<Tuple>>;
  for_each_impl(std::forward<Tuple>(tuple), std::forward<Func>(FUNC),
                std::make_index_sequence<SIZE>{});
}
}  // namespace proto