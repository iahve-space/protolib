#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "protocols/lacte/Helpers.hpp"
namespace proto::lacte {

using namespace std::string_view_literals;
#pragma pack(push, 1)
struct McuUid {
  constexpr static auto UID_SIZE = 12;
  std::array<uint8_t, UID_SIZE> m_data{};
  McuUid() = default;
  McuUid(const std::initializer_list<uint8_t> INIT) {
    std::fill(std::begin(m_data), std::end(m_data), 0);
    const size_t SIZE = std::min(INIT.size(), sizeof(m_data));
    std::copy_n(INIT.begin(), SIZE, m_data.data());
  }
  explicit McuUid(std::string_view STR_VIEW) {
    parse_hex_bytes_fill(STR_VIEW, m_data.data(), sizeof m_data);
  }
  explicit McuUid(const std::string& STR) : McuUid(std::string_view{STR}) {}
  auto operator==(const McuUid& other) const -> bool {
    return std::memcmp(m_data.data(), other.m_data.data(), sizeof(m_data)) == 0;
  }
  auto operator!=(const McuUid& other) const -> bool {
    return !(*this == other);
  }
  constexpr static std::string_view NAME = "McuUid";

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream oss;
    print_bytes(oss, m_data.data(), sizeof m_data);
    return oss.str();
  }
  [[nodiscard]] explicit operator std::string() const {
    return this->to_string();
  }

  friend auto operator<<(std::ostream& out, const McuUid& val)
      -> std::ostream& {
    return out << NAME << ": " << std::string(val);
  }
};

inline auto to_string(const McuUid& val) -> std::string {
  return val.to_string();
}
#pragma pack(pop)

static_assert(sizeof(McuUid) == McuUid::UID_SIZE, "McuUid must be 12 bytes");
}  // namespace proto::lacte