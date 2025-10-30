#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "CustomSpan.hpp"

namespace proto::interface {
using CallbackType =
    std::function<void(CustomSpan<uint8_t> buffer, size_t &read)>;
using Delegate = std::shared_ptr<CallbackType>;

using namespace std::chrono_literals;

class IInterface {
 public:
  virtual ~IInterface() = default;
  explicit IInterface(const char *name) : m_name(name) {};
  virtual auto write(CustomSpan<uint8_t> buffer,
                     std::chrono::milliseconds timeout = 1s) -> bool = 0;

  virtual auto is_open() -> bool = 0;
  virtual auto open() -> bool = 0;
  virtual auto close() -> bool = 0;
  [[nodiscard]] virtual auto add_receive_callback(const CallbackType &callback)
      -> Delegate {
    auto callback_ = std::make_shared<CallbackType>(callback);
    m_callbacks.push_back(callback_);
    return callback_;
  }

 protected:
  std::string m_name;
  std::vector<std::weak_ptr<CallbackType>> m_callbacks;

 private:
  virtual auto read(uint8_t *buffer, size_t count) -> int = 0;
};
}  // namespace proto::interface
