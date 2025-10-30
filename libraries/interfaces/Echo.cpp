#include "Echo.hpp"

#include <iostream>

namespace proto::interface {

auto EchoInterface::write(CustomSpan<uint8_t> buffer,
                          std::chrono::milliseconds timeout) -> bool {
  (void)timeout;
  if (!is_open_) {
    std::cerr << "Interface is not Open!" << '\n';
    return false;
  }
  std::lock_guard lock(write_mtx_);
  size_t read = 0;
  for (int i = static_cast<int>(m_callbacks.size() - 1); i >= 0; i--) {
    if (auto callback = m_callbacks[i].lock(); !callback) {
      m_callbacks.erase(m_callbacks.begin() + i);
    } else {
      (*callback)(buffer.subspan(read), read);
    }
  }
  return true;
}

auto EchoInterface::is_open() -> bool { return is_open_; }

auto EchoInterface::open() -> bool {
  is_open_ = true;
  return true;
}

auto EchoInterface::close() -> bool {
  is_open_ = false;
  return true;
}

auto EchoInterface::read(uint8_t *buffer, size_t count) -> int {
  (void)buffer;
  (void)count;
  return 0;
}
}  // namespace proto::interface