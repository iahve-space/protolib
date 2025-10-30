#pragma once
#include <protocols/lacte/objects/LacteBoardErrors.hpp>
#include <protocols/lacte/objects/RfidNumberType.hpp>
#include <algorithm>

namespace proto::lacte {

#pragma pack(push, 1)

enum class BoardStatus : std::uint8_t {
  IDLE = 0,
  CALIBRATION = 1,
  ERROR = 2,
  READY = 3,
  WORK = 4
};

static std::vector<std::string> all_board_statuses{
    {"IDLE", "CALIBRATION", "ERROR", "READY", "WORK"}};

[[nodiscard]] inline auto to_string(const BoardStatus STR) noexcept
    -> std::string_view {
  return all_board_statuses[static_cast<std::underlying_type_t<BoardStatus>>(
      STR)];
}

inline auto operator<<(std::ostream& out, const BoardStatus STATUS)
    -> std::ostream& {
  if (const auto STR_VIEW = to_string(STATUS); !STR_VIEW.empty()) {
    return out << STR_VIEW;
  }
  return out << "UNKNOWN(" << static_cast<unsigned>(STATUS) << ')';
}

struct InfoPacketType {
  constexpr static auto NAME = "InfoPacket"sv;
  BoardStatus m_status{};
  ErrorFlags m_errors;
  RFIDNumberType m_rfid;

  InfoPacketType() = default;
  explicit InfoPacketType(const BoardStatus STATUS, const ErrorFlags ERRORS,
                          const RFIDNumberType RFID)
      : m_status(STATUS), m_errors(ERRORS), m_rfid(RFID) {}
  explicit InfoPacketType(const std::string_view CONFIG) {
    const std::string_view STATUS_STR = CONFIG.substr(0, CONFIG.find(','));
    const std::string_view ERRORS_STR = CONFIG.substr(
        CONFIG.find(',') + 1, CONFIG.rfind(',') - CONFIG.find(',') - 1);
    const std::string_view RFID_STR = CONFIG.substr(CONFIG.rfind(',') + 1);
    m_status = std::find(all_board_statuses.begin(), all_board_statuses.end(),
                         STATUS_STR) != all_board_statuses.end()
                   ? static_cast<BoardStatus>(std::distance(
                         all_board_statuses.begin(),
                         std::find(all_board_statuses.begin(),
                                   all_board_statuses.end(), STATUS_STR)))
                   : BoardStatus::IDLE;
    m_errors = ErrorFlags(ERRORS_STR);
    m_rfid = RFIDNumberType(RFID_STR);
  }

  friend auto operator<<(std::ostream& out, const InfoPacketType& INFO)
      -> std::ostream& {
    const auto FLAGS = out.flags();  // форматные флаги
    // Печатаем, как голые байты UTF-8, с CRLF и явными разделителями
    out << "InfoPacket: " << INFO.to_string();
    out.flags(FLAGS);
    return out;
  }

  auto operator==(const InfoPacketType& other) const -> bool {
    return (m_status == other.m_status) && (m_rfid == other.m_rfid);
  }

  auto operator!=(const InfoPacketType& other) const -> bool {
    return (m_status != other.m_status) || (m_rfid != other.m_rfid);
  }

  [[nodiscard]] auto to_string() const -> std::string {
    std::ostringstream out;
    // Печатаем как голые байты UTF-8, с CRLF и явными разделителями
    out << m_status << "," << m_errors << "," << m_rfid;
    return out.str();
  }
};

#pragma pack(pop)
}  // namespace proto::lacte