#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

namespace proto::lacte {

using namespace std::string_view_literals;
#pragma pack(push, 1)

struct RFIDNumberType {
  explicit RFIDNumberType(uint64_t new_id = 0) {
    memset(id_.data(), 0, sizeof id_);
    memcpy(id_.data(), (uint8_t*)&new_id, sizeof id_);
  }
  explicit RFIDNumberType(std::string_view text) {
    // Normalize input: trim spaces and optional separators, detect base
    std::string str;
    str.reserve(text.size());
    for (const char VAL : text) {
      if (VAL == ' ' || VAL == '\t' || VAL == '\n' || VAL == ':' ||
          VAL == '-') {
        continue;  // ignore separators
      }
      str.push_back(VAL);
    }

    int base = DEC_BASE;
    if (str.size() > 2 && (str[0] == '0') && (str[1] == 'x' || str[1] == 'X')) {
      base = HEX_BASE;
    } else {
      // Heuristic: if contains hex letters, treat as hex
      for (const char VAL : str) {
        if ((VAL >= 'a' && VAL <= 'f') || (VAL >= 'A' && VAL <= 'F')) {
          base = HEX_BASE;
          break;
        }
      }
    }

    // std::stout handles both with/without 0x when base==0, but we choose
    // explicit base for clarity
    unsigned long long value = 0;
    try {
      size_t idx = 0;
      value = std::stoull(str, &idx, base);
      (void)
          idx;  // if idx < s.size(), trailing chars are ignored by design above
    } catch (...) {
      value = 0;  // fallback to zero on parse failure
    }

    // Store lower 56 bits in little-endian order to match the uint64_t ctor
    // behavior
    std::memset(id_.data(), 0, sizeof id_);
    const uint64_t V64 = static_cast<uint64_t>(value) & 0x00FFFFFFFFFFFFFFULL;
    std::memcpy(id_.data(), reinterpret_cast<const uint8_t*>(&V64), sizeof id_);
  }
  explicit RFIDNumberType(const std::string& text)
      : RFIDNumberType(std::string_view{text}) {}
  constexpr static std::string_view NAME = "RFID"sv;
  auto operator==(const RFIDNumberType& other) const -> bool {
    return memcmp(id_.data(), other.id_.data(), sizeof id_) == 0;
  }
  auto operator!=(const RFIDNumberType& other) const -> bool {
    return memcmp(id_.data(), other.id_.data(), sizeof id_) != 0;
  }

  // вариант 1: явная функция конвертации в строку
  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream oss;
    const uint8_t* data = id_.data();
    uint64_t raw;
    std::memcpy(&raw, data, sizeof(raw));
    const uint64_t RESULT = raw & 0xFFFFFFFFFFFFFFULL;

    oss << RESULT;
    return oss.str();
  }

  // вариант 2: оператор преобразования (тогда можно писать (std::string)v)
  explicit operator std::string()
      const {  // explicit, чтобы не было неявных сюрпризов
    return to_string();
  }

  friend auto operator<<(std::ostream& out, const RFIDNumberType& VAL)
      -> std::ostream& {
    out << "RFID: ";
    out << VAL.to_string();
    return out;
    // или: return os << "\nRFIDPacket ID: " << v.to_string();
  }

 private:
  constexpr static uint8_t DEC_BASE = 10;
  constexpr static uint8_t HEX_BASE = 16;
  constexpr static uint8_t ID_LEN = 7;
  std::array<uint8_t, ID_LEN> id_{};
};

// вариант 1: явная функция конвертации в строку
static auto to_string(const RFIDNumberType& rfid) -> std::string {
  std::ostringstream oss;
  oss << rfid.to_string();
  return oss.str();
}

#pragma pack(pop)

}  // namespace proto::lacte