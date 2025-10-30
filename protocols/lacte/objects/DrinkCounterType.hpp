#pragma once
#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

#pragma pack(push, 1)
struct DrinkCounter {
  uint32_t m_data{};
  DrinkCounter() = default;
  explicit DrinkCounter(uint32_t val) : m_data(val) {}
  explicit DrinkCounter(const std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit DrinkCounter(const std::string& STR)
      : DrinkCounter(std::string_view{STR}) {}
  auto operator==(const DrinkCounter& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const DrinkCounter& other) const -> bool {
    return !(*this == other);
  }
  constexpr static std::string_view NAME = "DrinkCounter";

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    out << m_data;
    return out.str();
  }
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  friend auto operator<<(std::ostream& out, const DrinkCounter& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};

inline auto to_string(const DrinkCounter& val) -> std::string {
  return val.to_string();
}

#pragma pack(pop)

static_assert(sizeof(DrinkCounter) == 4, "DrinkCounter must be 4 bytes");
}