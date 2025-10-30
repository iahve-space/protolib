#pragma once

#include <mutex>
#include <vector>

#include "CustomSpan.hpp"
#include "Interface.hpp"

namespace proto::interface {
class EchoInterface final : public IInterface {
 public:
  explicit EchoInterface() : IInterface("echo interface") {}

  auto write(CustomSpan<uint8_t> buffer, std::chrono::milliseconds timeout)
      -> bool override;

  auto is_open() -> bool override;

  auto open() -> bool override;

  auto close() -> bool override;

 private:
  std::mutex write_mtx_;
  bool is_open_{false};
  std::vector<uint8_t> receive_buffer_;
  auto read(uint8_t* buffer, size_t count) -> int override;
};
}  // namespace proto::interface