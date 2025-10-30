#pragma once
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include "protocols/lacte/Helpers.hpp"
#include "protocols/lacte/objects/McuUidType.hpp"

namespace proto::lacte {

#pragma pack(push, 1)
struct UIDPacketType {
  McuUid m_uid;
  constexpr static std::string_view NAME = "UID"sv;

  UIDPacketType() = default;
  UIDPacketType(const std::initializer_list<uint8_t> DATA) {
    m_uid = McuUid(DATA);
  }
  // Construct from hex string (robust parser)
  explicit UIDPacketType(const std::string_view STR_VIEW) {
    m_uid = McuUid(STR_VIEW);
  }

  auto operator==(const UIDPacketType& other) const noexcept -> bool {
    return m_uid == other.m_uid;
  }
  auto operator!=(const UIDPacketType& other) const noexcept -> bool {
    return !(*this == other);
  }

  // --- внутренняя to_string: только значение ---
  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream oss;
    print_bytes(oss, m_uid.m_data.data(), sizeof(m_uid));
    return oss.str();
  }

  // --- приведение к std::string: делает то же, что и to_string() ---
  [[nodiscard]] operator std::string()
      const {  // NOLINT(google-explicit-constructor)
    return this->to_string();
  }

  // --- вывод в поток: имя + значение ---
  friend auto operator<<(std::ostream& out, const UIDPacketType& VALUE)
      -> std::ostream& {
    out << "UIDPacket: ";
    print_bytes(out, VALUE.m_uid.m_data.data(), sizeof(VALUE.m_uid));
    return out;
  }
};

// --- внешняя to_string: то же, что и внутренняя ---
inline auto to_string(const UIDPacketType& VAL) -> std::string {
  return VAL.to_string();
}

#pragma pack(pop)
}  // namespace proto::lacte