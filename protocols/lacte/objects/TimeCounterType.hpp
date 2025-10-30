#pragma once
#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

#pragma pack(push, 1)
struct TimeCounter {
  uint32_t m_data{};
  TimeCounter() = default;
  explicit TimeCounter(const uint32_t VAL) : m_data(VAL) {}
  explicit TimeCounter(const std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit TimeCounter(const std::string& STR)
      : TimeCounter(std::string_view{STR}) {}
  auto operator==(const TimeCounter& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const TimeCounter& other) const -> bool {
    return !(*this == other);
  }
  constexpr static std::string_view NAME = "TimeCounter";

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    out << m_data;
    return out.str();
  }
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  friend auto operator<<(std::ostream& out, const TimeCounter& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};
inline auto to_string(const TimeCounter& val) -> std::string {
  return val.to_string();
}

#pragma pack(pop)

static_assert(sizeof(TimeCounter) == 4, "TimeCounter must be 4 bytes");
}