#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace proto::lacte {

using namespace std::string_view_literals;

#pragma pack(push, 1)
struct VersionPacketType {
  struct Version {
    int m_major;
    int m_minor;
  };
  constexpr static std::string_view NAME = "Version"sv;
  uint8_t m_major;
  uint8_t m_minor;
  VersionPacketType() = default;
  explicit VersionPacketType(const Version VERS)
      : m_major(VERS.m_major), m_minor(VERS.m_minor) {}
  // Construct from "1.2" style string
  explicit VersionPacketType(std::string_view str_view) {
    auto dot = str_view.find('.');
    if (dot != std::string_view::npos) {
      m_major =
          static_cast<uint8_t>(std::stoi(std::string(str_view.substr(0, dot))));
      m_minor = static_cast<uint8_t>(
          std::stoi(std::string(str_view.substr(dot + 1))));
    } else {
      // fallback: treat whole string as major, minor = 0
      m_major = static_cast<uint8_t>(std::stoi(std::string(str_view)));
      m_minor = 0;
    }
  }
  auto operator==(const VersionPacketType& other) const -> bool {
    return m_major == other.m_major && m_minor == other.m_minor;
  }
  explicit operator std::string() const {
    std::string result(std::to_string(m_major) + '.' + std::to_string(m_minor));
    return result;
  }
  [[nodiscard]] auto to_string() const -> std::string {
    return static_cast<std::string>(*this);
  }
  friend auto operator<<(std::ostream& out, const VersionPacketType& VERSION)
      -> std::ostream& {
    out << "\nVersion: " << static_cast<std::string>(VERSION) << '\n';
    return out;
  }
};
inline auto to_string(const VersionPacketType& VERS) -> std::string {
  return VERS.to_string();
}
#pragma pack(pop)
}  // namespace proto::lacte