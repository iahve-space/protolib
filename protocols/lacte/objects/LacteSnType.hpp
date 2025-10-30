#pragma once
#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

#pragma pack(push, 1)
struct LacteSn {
  constexpr static auto NAME = "LacteSn"sv;
  uint32_t m_data{};
  LacteSn() = default;
  explicit LacteSn(const uint32_t VAL) : m_data(VAL) {}
  explicit LacteSn(const std::string_view STR_VIEW) {
    m_data = static_cast<uint32_t>(parse_uint_sv(STR_VIEW));
  }
  explicit LacteSn(const std::string& STR) : LacteSn(std::string_view{STR}) {}
  auto operator==(const LacteSn& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const LacteSn& other) const -> bool {
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

  friend auto operator<<(std::ostream& out, const LacteSn& VAL)
      -> std::ostream& {
    return out << NAME << ": " << std::string(VAL);
  }
};

inline auto to_string(const LacteSn& VAL) -> std::string {
  return VAL.to_string();
}
#pragma pack(pop)

static_assert(sizeof(LacteSn) == 4, "LacteSn must be 4 bytes");
}