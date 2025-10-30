#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace std::string_view_literals;

namespace proto::lacte {
// Флаги как МАСКИ (а не как номера битов) — читать проще.
enum class BoardError : uint8_t {
  CALIBRATION = 1U << 0,    // ERROR_FLAG_CALIB_ERROR
  MOTOR = 1U << 1,          // ERROR_FLAG_MOTOR_ERROR
  RFID = 1U << 2,           // ERROR_FLAG_RFID_ERROR
  RFID_NO_CARD = 1U << 3,   // ERROR_FLAG_RFID_NO_CARD
  RFID_BAD_CARD = 1U << 4,  // ERROR_FLAG_RFID_BAD_CARD
  INVALID_TIME =
      1U << 5,  // ERROR_FLAG_INVALID_TIME время на устройстве не
                // синхронизировано, все операции по наливу заблокированы
  SHELF_LIFE_EXCEEDED =
      1U << 6,  // ERROR_FLAG_SHELF_LIFE_EXCEEDED срок годности продукта истек
  USAGE_TIME_EXCEEDED =
      1U
      << 7,  // ERROR_FLAG_USAGE_TIME_EXCEEDED срок эксплуатации продукта истек

};

inline auto to_string(const BoardError ERROR) -> std::string_view {
  switch (ERROR) {
    case BoardError::CALIBRATION:
      return "CalibrationError"sv;
    case BoardError::MOTOR:
      return "MotorError"sv;
    case BoardError::RFID:
      return "RFIDError"sv;
    case BoardError::RFID_NO_CARD:
      return "RFID_NoCard"sv;
    case BoardError::RFID_BAD_CARD:
      return "RFID_BadCard"sv;
    case BoardError::INVALID_TIME:
      return "InvalidTime"sv;
    case BoardError::SHELF_LIFE_EXCEEDED:
      return "ShelfLifeExceeded"sv;
    case BoardError::USAGE_TIME_EXCEEDED:
      return "UsageTimeExceeded"sv;
  }
  return "UnknownError"sv;
}

inline auto operator<<(std::ostream& out, const BoardError ERROR)
    -> std::ostream& {
  return out << to_string(ERROR);
}

constexpr auto mask(BoardError ERROR) -> uint16_t {
  return static_cast<uint16_t>(ERROR);
}

constexpr std::array<BoardError, 8> ALL_BOARD_ERRORS = {
    BoardError::CALIBRATION,
    BoardError::MOTOR,
    BoardError::RFID,
    BoardError::RFID_NO_CARD,
    BoardError::RFID_BAD_CARD,
    BoardError::INVALID_TIME,
    BoardError::SHELF_LIFE_EXCEEDED,
    BoardError::USAGE_TIME_EXCEEDED};

#pragma pack(push, 1)
class ErrorFlags {
 public:
  constexpr static auto NAME = "BoardErrors"sv;
  constexpr ErrorFlags() = default;
  explicit constexpr ErrorFlags(const uint16_t BITS) : m_bits(BITS) {}

  // Удобный фабричный конструктор из набора флагов.
  constexpr ErrorFlags(const std::initializer_list<BoardError> INIT) {
    for (const auto ERROR : INIT) {
      m_bits |= mask(ERROR);
    }
  }

  // Construct from string produced by to_string(ErrorFlags)
  explicit ErrorFlags(const std::string_view STR) {
    // naive token search by names
    for (const auto ERROR : ALL_BOARD_ERRORS) {
      if (STR.find(to_string(ERROR)) != std::string_view::npos) {
        set(ERROR);
      }
    }
  }

  // Сырые биты внутрь/наружу
  static constexpr auto from_raw(const uint16_t VAL) -> ErrorFlags {
    return ErrorFlags{VAL};
  }
  [[nodiscard]] constexpr auto value() const -> uint16_t { return m_bits; }

  // Проверки
  [[nodiscard]] constexpr auto has(const BoardError ERROR) const -> bool {
    return (m_bits & mask(ERROR)) != 0;
  }
  [[nodiscard]] constexpr auto any() const -> bool { return m_bits != 0; }
  [[nodiscard]] constexpr auto none() const -> bool { return m_bits == 0; }

  // Количество установленных флагов
  [[nodiscard]] auto count() const -> int {
#if __cplusplus >= 202002L
    return std::popcount(bits_);
#else
    // Fallback для док C++17
    uint16_t val = m_bits;
    int character = 0;
    while (val != 0U) {
      val &= (val - 1);
      ++character;
    }
    return character;
#endif
  }

  // Мутации
  void set(const BoardError ERROR) { m_bits |= mask(ERROR); }
  void clear(const BoardError ERROR) { m_bits &= ~mask(ERROR); }
  void reset() { m_bits = 0; }

  // Список выставленных ошибок — удобно для UI/логов
  [[nodiscard]] auto list() const -> std::vector<BoardError> {
    std::vector<BoardError> out;
    for (auto ERROR : ALL_BOARD_ERRORS) {
      if (has(ERROR)) {
        out.push_back(ERROR);
      }
    }
    return out;
  }

  auto operator==(const ErrorFlags OTHER) const -> bool {
    return m_bits == OTHER.m_bits;
  }
  auto operator!=(const ErrorFlags OTHER) const -> bool {
    return m_bits != OTHER.m_bits;
  }
  uint16_t m_bits = 0;
};
#pragma pack(pop)

inline auto to_string(const ErrorFlags& ERRORS) -> std::string {
  std::stringstream out;
  if (ERRORS.none()) {
    return "Errors: NONE";
  }
  out << "Errors(" << ERRORS.count() << "): ";
  bool first = true;
  for (const auto ERROR : ERRORS.list()) {
    if (!first) {
      out << ", ";
    }
    first = false;
    out << to_string(ERROR);  // воспользуется operator<<(BoardError)
  }
  out << " [0x" << std::hex << ERRORS.value() << std::dec << "]";
  std::string result = out.str();
  return result;
}

inline auto operator<<(std::ostream& out, const ErrorFlags& ERRORS)
    -> std::ostream& {
  return out << to_string(ERRORS);
}

class BoardErrorDiff {
 public:
  constexpr static auto NAME = "BoardErrorDiff"sv;
  std::unordered_map<BoardError, std::optional<bool>> m_errors;
  explicit BoardErrorDiff(const ErrorFlags& ERRORS) {
    for (auto error : ALL_BOARD_ERRORS) {
      bool value = ERRORS.has(error);
      m_errors[error] = value;
    }
  }
  explicit BoardErrorDiff(const ErrorFlags&& ERRORS) {
    for (auto error : ALL_BOARD_ERRORS) {
      bool value = ERRORS.has(error);
      m_errors[error] = value;
    }
  }
  explicit BoardErrorDiff(const std::string_view ERRORS_STR) {
    for (auto error : ALL_BOARD_ERRORS) {
      // Ищем имя ошибки в строке
      if (const auto ERROR_STR = lacte::to_string(error);
          ERRORS_STR.find(ERROR_STR) != std::string_view::npos) {
        m_errors[error] = true;
      } else {
        m_errors[error] = false;
      }
    }
  }

  BoardErrorDiff(const BoardErrorDiff& ERRORS) = default;
  BoardErrorDiff(BoardErrorDiff&& Errors) noexcept = default;

  auto operator=(const BoardErrorDiff& OTHER) -> BoardErrorDiff& {
    for (const auto ERROR : ALL_BOARD_ERRORS) {
      m_errors[ERROR] = (m_errors[ERROR] != OTHER.m_errors.at(ERROR))
                            ? OTHER.m_errors.at(ERROR)
                            : std::nullopt;
    }
    return *this;
  }
  auto operator=(const BoardErrorDiff&& OTHER) noexcept -> BoardErrorDiff& {
    for (const auto ERROR : ALL_BOARD_ERRORS) {
      m_errors[ERROR] = OTHER.m_errors.at(ERROR);
    }
    return *this;
  }
  auto operator==(const BoardErrorDiff& OTHER) const -> bool {
    (void)OTHER;
    return false;
  }
  auto operator!=(const BoardErrorDiff& OTHER) const -> bool {
    (void)OTHER;
    return true;
  }

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    for (const auto ERROR : ALL_BOARD_ERRORS) {
      if (m_errors.at(ERROR).has_value()) {
        out << proto::lacte::to_string(ERROR) << "="
            << (m_errors.at(ERROR).value_or(false) ? "SET" : "CLEARED") << "; ";
      }
    }
    return out.str();
  }

  friend auto operator<<(std::ostream& out, const BoardErrorDiff& BOARD)
      -> std::ostream& {
    out << NAME << BOARD.to_string();
    return out;
  }
};
}  // namespace proto::lacte