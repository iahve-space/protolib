#pragma once
#include <cstdint>

#include "CustomSpan.hpp"

class ICrc {
 public:
  virtual ~ICrc() = default;
  explicit ICrc(const char *name) : m_name(name) {}
  const char *m_name{};
  virtual void reset() = 0;
  virtual auto calc(CustomSpan<uint8_t>) -> uint32_t = 0;
  virtual auto append(uint32_t, CustomSpan<uint8_t>) -> uint32_t = 0;
};
