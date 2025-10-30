#pragma once

#include <cstdint>

namespace proto::lacte {

#pragma pack(push, 1)

enum class BootAnswer : uint8_t { NO = 0, YES = 1 };

struct BootAnswerType {
  BootAnswer m_data{};
  auto operator==(const BootAnswerType& other) const -> bool {
    return m_data == other.m_data;
  }
};

#pragma pack(pop)
}  // namespace proto::lacte