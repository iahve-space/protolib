#pragma once
#include "protocols/lacte/Helpers.hpp"

namespace proto::lacte {

#pragma pack(push, 1)

struct MagicWord {
  static constexpr uint16_t DEFAULT_VAL = 0xccaa;
  uint16_t m_data{};
  MagicWord() = default;
  explicit MagicWord(const uint16_t VAL) : m_data(VAL) {};
  explicit MagicWord(const std::string_view STR_VIEW) {
    m_data = static_cast<uint16_t>(parse_uint_sv(STR_VIEW));
  }
  explicit MagicWord(const std::string& STR)
      : MagicWord(std::string_view{STR}) {}
  auto operator==(const MagicWord& other) const -> bool {
    return m_data == other.m_data;
  }
  auto operator!=(const MagicWord& other) const -> bool {
    return !(*this == other);
  }
  constexpr static std::string_view NAME = "MagicWord";

  // только значение
  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    out << std::dec << static_cast<unsigned>(m_data);
    return out.str();
  }
  // приведение к строке = to_string()
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  // имя + значение
  friend auto operator<<(std::ostream& out, const MagicWord& VAL)
      -> std::ostream& {
    return out << NAME << ": " << std::string(VAL);
  }
};

inline auto to_string(const MagicWord& VAL) -> std::string {
  return VAL.to_string();
}
#pragma pack(pop)

static_assert(sizeof(MagicWord) == 2, "MagicWord must be 2 bytes");
}  // namespace proto::lacte
