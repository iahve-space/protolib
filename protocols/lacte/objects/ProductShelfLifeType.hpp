#pragma once
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

using namespace std::literals::string_view_literals;

#pragma pack(push, 1)
struct ProductShelfLife {
  constexpr static auto NAME = "ProductShelfLife"sv;
  uint32_t m_data{};
  ProductShelfLife() = default;
  explicit ProductShelfLife(const uint32_t VAL) : m_data(VAL) {}
  explicit ProductShelfLife(const std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit ProductShelfLife(const std::string& STR)
      : ProductShelfLife(std::string_view{STR}) {}
  auto operator==(const ProductShelfLife& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const ProductShelfLife& other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    out << m_data;
    return out.str();
  }
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  friend auto operator<<(std::ostream& out, const ProductShelfLife& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};

inline auto to_string(const ProductShelfLife& val) -> std::string {
  return val.to_string();
}
#pragma pack(pop)

static_assert(sizeof(ProductShelfLife) == 4,
              "ProductShelfLife must be 4 bytes");
}  // namespace proto::lacte