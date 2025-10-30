#pragma once
#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

#pragma pack(push, 1)
struct ProdDate {
  uint32_t m_data{};
  ProdDate() = default;
  explicit ProdDate(uint32_t val) : m_data(val) {}
  explicit ProdDate(std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit ProdDate(const std::string& STR) : ProdDate(std::string_view{STR}) {}
  auto operator==(const ProdDate& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const ProdDate& other) const -> bool {
    return !(*this == other);
  }
  constexpr static std::string_view NAME = "ProdDate";

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    out << m_data;
    return out.str();
  }
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  friend auto operator<<(std::ostream& out, const ProdDate& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};

inline auto to_string(const ProdDate& val) -> std::string {
  return val.to_string();
}

#pragma pack(pop)

static_assert(sizeof(ProdDate) == 4, "ProdDate must be 4 bytes");
}