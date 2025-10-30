#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

using namespace std::literals::string_view_literals;

#pragma pack(push, 1)
struct ProductVolume {
  constexpr static auto NAME = "ProductVolume"sv;
  uint16_t m_data{};
  ProductVolume() = default;
  explicit ProductVolume(const uint16_t VAL) : m_data(VAL) {}
  explicit ProductVolume(const std::string_view STR_VIEW) {
    m_data = static_cast<uint16_t>(parse_uint_sv(STR_VIEW));
  }
  explicit ProductVolume(const std::string& STR)
      : ProductVolume(std::string_view{STR}) {}
  auto operator==(const ProductVolume& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const ProductVolume& other) const -> bool {
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

  friend auto operator<<(std::ostream& out, const ProductVolume& VAL)
      -> std::ostream& {
    return out << NAME << ": " << std::string(VAL);
  }
};

inline auto to_string(const ProductVolume& VAL) -> std::string {
  return VAL.to_string();
}
#pragma pack(pop)

static_assert(sizeof(ProductVolume) == 2, "ProductVolume must be 2 bytes");
}  // namespace proto::lacte