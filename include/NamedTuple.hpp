// named_tuple.hpp
#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

#include "prototypes/field/FieldPrototype.hpp"

namespace proto::meta {

template <FieldName NAME, class Tuple, std::size_t I = 0>
constexpr auto get_named(Tuple& val) -> decltype(auto) {
  static_assert(I < std::tuple_size_v<std::remove_reference_t<Tuple>>,
                "Field with this NAME not found in NamedReturnTuple");
  using Elem = std::tuple_element_t<I, std::remove_reference_t<Tuple>>;
  if constexpr (Elem::NAME == NAME) {
    return (std::get<I>(val).m_value);
  } else {
    return get_named<NAME, Tuple, I + 1>(val);
  }
}

// const-перегрузка (если нужна)
template <FieldName NAME, class Tuple, std::size_t I = 0>
constexpr auto get_named(const Tuple& val) -> decltype(auto) {
  static_assert(I < std::tuple_size_v<std::remove_reference_t<Tuple>>,
                "Field with this NAME not found in NamedReturnTuple");
  using Elem = std::tuple_element_t<I, std::remove_reference_t<Tuple>>;
  if constexpr (Elem::NAME == NAME) {
    return (std::get<I>(val).m_value);
  } else {
    return get_named<NAME, Tuple, I + 1>(val);
  }
}

}  // namespace proto::meta
