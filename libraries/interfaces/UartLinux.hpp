#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "CustomSpan.hpp"
#include "Interface.hpp"

namespace proto::interface {
class UartLinuxInterface final : public IInterface {
 public:
  explicit UartLinuxInterface() : IInterface("uart linux interface") {}
  ~UartLinuxInterface() override {
    is_open_ = false;
    if (receive_thread_.joinable()) {
      receive_thread_.join();
    }
  }
  auto write(CustomSpan<uint8_t> /*buffer*/,
             std::chrono::milliseconds /*timeout*/) -> bool override;

  auto is_open() -> bool override;

  auto open() -> bool override;

  auto open_uart(const std::string& device, int baudrate) -> int;

  auto open_uart(const std::string& vid, const std::string& pid, int BAUDRATE)
      -> int;

  auto close() -> bool override;

 private:
  int fd_{-1};
  int baudrate_{};
  std::string vid_pid_;
  std::mutex write_mtx_;
  std::mutex read_mtx_;
  std::atomic_bool is_open_{false};

  std::array<uint8_t, 1000> receive_buffer_{};

  std::thread receive_thread_;

  auto uart_reader_thread() -> int;
  auto read(uint8_t* /*buffer*/, size_t /*count*/) -> int override;
};
}  // namespace proto::interface