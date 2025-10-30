#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

using namespace std::string_view_literals;

#pragma pack(push, 1)
struct UsageTime {
  constexpr static std::string_view NAME = "UsageTime"sv;
  uint32_t m_data{};
  UsageTime() = default;
  explicit UsageTime(const uint32_t VAL) : m_data(VAL) {}
  explicit UsageTime(const std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit UsageTime(const std::string& STR)
      : UsageTime(std::string_view{STR}) {}
  auto operator==(const UsageTime& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const UsageTime& other) const -> bool {
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

  friend auto operator<<(std::ostream& out, const UsageTime& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};

inline auto to_string(const UsageTime& val) -> std::string {
  return val.to_string();
}
#pragma pack(pop)

static_assert(sizeof(UsageTime) == 4, "UsageTime must be 2 bytes");
}  // namespace proto::lacte