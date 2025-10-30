#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

using namespace std::literals::string_view_literals;

#pragma pack(push, 1)
struct LacteId {
  constexpr static auto NAME = "LacteId"sv;
  uint16_t m_data{};
  LacteId() = default;
  explicit LacteId(const uint32_t VAL) : m_data(static_cast<uint16_t>(VAL)) {}
  explicit LacteId(const std::string_view STR_VIEW) {
    m_data = static_cast<uint16_t>(parse_uint_sv(STR_VIEW));
  }
  explicit LacteId(const std::string& STR) : LacteId(std::string_view{STR}) {}
  auto operator==(const LacteId& other) const -> bool {
    return other.m_data == m_data;
  }
  auto operator!=(const LacteId& other) const -> bool {
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

  friend auto operator<<(std::ostream& out, const LacteId& VAL)
      -> std::ostream& {
    return out << NAME << ": " << std::string(VAL);
  }
};

inline auto to_string(const LacteId& VAL) -> std::string {
  return VAL.to_string();
}
#pragma pack(pop)

static_assert(sizeof(LacteId) == 2, "LacteId must be 2 bytes");
}  // namespace proto::lacte