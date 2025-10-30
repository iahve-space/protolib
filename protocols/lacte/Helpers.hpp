#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace proto::lacte {
using namespace std::string_view_literals;

// helpers for parsing from string_view
inline auto parse_uint_sv(std::string_view string_view) -> uint64_t {
  // trim spaces
  const auto LEFT = string_view.find_first_not_of(" \t\n\r");
  const auto RIGHT = string_view.find_last_not_of(" \t\n\r");
  if (LEFT == std::string_view::npos) {
    return 0;
  }
  string_view = string_view.substr(LEFT, RIGHT - LEFT + 1);

  // remove common separators for convenience
  std::string str;
  str.reserve(string_view.size());
  for (const char VAL : string_view) {
    if (VAL == ' ' || VAL == '\t' || VAL == '\n' || VAL == '_') {
      continue;
    }
    str.push_back(VAL);
  }
  constexpr static uint8_t DECIMAL_BASE = 10;
  constexpr static uint8_t HEX_BASE = 16;
  int base = DECIMAL_BASE;
  if (str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    base = HEX_BASE;
  } else {
    for (const char VAL : str) {
      if ((VAL >= 'a' && VAL <= 'f') || (VAL >= 'A' && VAL <= 'F')) {
        base = HEX_BASE;
        break;
      }
    }
  }

  try {
    size_t idx = 0;
    const unsigned long long VAL = std::stoull(str, &idx, base);
    (void)idx;
    return static_cast<uint64_t>(VAL);
  } catch (...) {
    return 0;
  }
}

inline void parse_hex_bytes_fill(const std::string_view STR_VIEW, uint8_t* out,
                                 const size_t BYTES) {
  constexpr static uint8_t DECIMAL_BASE = 10;
  std::string hex;
  hex.reserve(STR_VIEW.size());
  for (size_t i = 0; i < STR_VIEW.size(); ++i) {
    const char VAL = STR_VIEW[i];
    if (VAL == '0' && i + 1 < STR_VIEW.size() &&
        (STR_VIEW[i + 1] == 'x' || STR_VIEW[i + 1] == 'X')) {
      ++i;       // пропустить 'x'/'X'
      continue;  // и не добавлять '0' от префикса
    }
    if (std::isxdigit(static_cast<unsigned char>(VAL)) != 0) {
      hex.push_back(VAL);
    }
  }
  // дальше как было:
  if ((hex.size() % 2) != 0U) {
    hex.insert(hex.begin(), '0');
  }
  std::fill_n(out, BYTES, 0);
  const size_t PAIRS = std::min(BYTES, hex.size() / 2);
  for (size_t i = 0; i < PAIRS; ++i) {
    const auto HIGH = hex[2 * i];
    const auto LOW = hex[(2 * i) + 1];
    auto val = [](const char VAL) {
      if (VAL >= '0' && VAL <= '9') {
        return VAL - '0';
      }
      if (VAL >= 'a' && VAL <= 'f') {
        return DECIMAL_BASE + (VAL - 'a');
      }
      if (VAL >= 'A' && VAL <= 'F') {
        return DECIMAL_BASE + (VAL - 'A');
      }
      return 0;
    };
    out[i] = static_cast<uint8_t>((val(HIGH) << 4) | val(LOW));
  }
}

// ── утилиты вывода
// ────────────────────────────────────────────────────────────
inline void print_bytes(std::ostream& out, const uint8_t* data, size_t n) {
  std::streamsize old_w = out.width();
  const auto OLD = out.flags();
  const char OLD_DIFF = out.fill();

  out << "0x" << std::hex << std::setfill('0');
  for (size_t i = 0; i < n; ++i) {
    out << std::setw(2) << static_cast<int>(data[i]);
    //            if (i + 1 < n) os << ' ';
  }
  out.flags(OLD);
  out.width(old_w);
  out.fill(OLD_DIFF);
}

inline void print_ts(std::ostream& out, const uint32_t TIME_STAMP) {
  constexpr static uint8_t MAX_STAMP_SIZE = 32;
  auto time = static_cast<std::time_t>(TIME_STAMP);
  std::tm sys_time{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(&sys_time, &time);
#else
  localtime_r(&time, &sys_time);
#endif
  std::array<char, MAX_STAMP_SIZE> buf{};
  if (std::strftime(buf.data(), sizeof(buf), "%F %T", &sys_time) != 0U) {
    out << buf.data();
  } else {
    out << "<bad time>";
  }
}
}  // namespace proto::lacte