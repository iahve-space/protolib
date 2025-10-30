#pragma once
#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

#pragma pack(push, 1)
struct MachineSn {
  uint32_t m_data{};
  MachineSn() = default;
  explicit MachineSn(const uint32_t VAL) : m_data(VAL) {}
  explicit MachineSn(const std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit MachineSn(const std::string& STR)
      : MachineSn(std::string_view{STR}) {}
  auto operator==(const MachineSn& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const MachineSn& other) const -> bool {
    return !(*this == other);
  }
  constexpr static std::string_view NAME = "MachineSn";

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    out << std::dec << std::setw(4) << m_data;
    return out.str();
  }
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  friend auto operator<<(std::ostream& out, const MachineSn& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};

inline auto to_string(const MachineSn& val) -> std::string {
  return val.to_string();
}
#pragma pack(pop)

static_assert(sizeof(MachineSn) == 4, "MachineSn must be 4 bytes");
}