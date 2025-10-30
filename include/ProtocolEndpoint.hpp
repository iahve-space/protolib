/**
 * @file ProtocolEndpoint.hpp
 * @brief Base endpoint class for protocol implementations.
 *
 * This header defines a generic ProtocolEndpoint template for managing protocol
 * communication endpoints. It provides containers for RX and TX fields, a
 * snapshot mechanism for the last received frame, and thread-safe access to
 * recent data.
 *
 * Usage:
 *   - Specialize with protocol field sets and CRC type.
 *   - Wire to external interfaces with SetInterfaces().
 *   - Use snapshot/query methods to access last received data.
 *
 * Thread-safety: All snapshot and wait methods are internally synchronized.
 *
 * Example:
 *   ProtocolEndpoint<MyRxFields, MyTxFields, MyCrc> ep;
 *   ep.SetInterfaces(rx_if, tx_if);
 *   // Send data, wait for responses, access last frame, etc.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "prototypes/container/RxContainer.hpp"
#include "prototypes/container/TxContainer.hpp"

namespace proto {

/**
 * @brief Protocol endpoint base for protocol implementations.
 *
 * @tparam RxFields Protocol RX field set.
 * @tparam TxFields Protocol TX field set.
 * @tparam Crc      CRC type for integrity.
 *
 * Responsibilities:
 *   - Handles RX/TX containers.
 *   - Snapshots the last received frame's type code (payload can be extracted
 * on-demand via wait_once).
 *   - Provides thread-safe access and waiting.
 *   - Allows user RX callback hooks.
 *
 * Lifecycle:
 *   - Construct, wire interfaces, use send/wait methods, query snapshot.
 */
template <class RxFields, class TxFields, class Crc = CrcSoft>
class ProtocolEndpoint {
 public:
  /** @brief RX container type for inbound protocol frames. */
  using RxCont = proto::RxContainer<RxFields, Crc>;
  /** @brief TX container type for outbound protocol frames. */
  using TxCont = proto::TxContainer<TxFields, Crc>;
  using ReceiveType = typename RxCont::ReturnType;
  using RxFieldsSnapshot = typename RxCont::NamedReturnTuple;

  /**
   * @brief Construct endpoint and install a permanent RX callback that
   * snapshots the last frame's type code.
   * @note A permanent RX callback is installed that snapshots the last received
   * type code.
   */
  explicit ProtocolEndpoint(bool debug = false) {
    set_debug(debug);
    // Permanent RX-callback
    // Permanent RX-callback that snapshots only the last type code.
    // NOTE: We intentionally do NOT materialize a variant for DATA_FIELD here,
    // to avoid duplicate-type variant instantiations (e.g., multiple uint8_t*
    // PacketInfo).
    m_rx_delegate = m_rx.add_receive_callback(m_rx_callback);
    m_deque_thread = std::thread([this] {
      std::unique_lock<std::mutex> lock(m_queue_mutex);
      while (true) {
        m_queue_cv.wait(lock,
                        [this] { return !m_running || !m_rx_queue.empty(); });
        if (!m_running) {
          break;
        }
        auto val = std::move(m_rx_queue.front());
        // вызываем без мьютекса
        lock.unlock();
        if (m_user_callback) {
          m_user_callback(std::move(val));
          m_rx_queue.pop_front();
        }
        lock.lock();
      }
    });
  }
  ~ProtocolEndpoint() {
    {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_running = false;  // если не atomic — тем более делать под мьютексом
    }
    m_queue_cv.notify_all();

    if (m_deque_thread.joinable()) {
      m_deque_thread.join();
    }
  }

  void set_debug(const bool VALUE) {
    m_rx.set_debug(VALUE);
    m_tx.set_debug(VALUE);
  }

  /**
   * @brief Wire the endpoint to external RX and TX interfaces.
   *
   * Sets up RX and TX containers to use the provided interfaces.
   * The RX callback feeds incoming byte chunks to rx_.Fill.
   *
   * @param rx_if Interface for receiving data.
   * @param tx_if Interface for transmitting data.
   */
  void set_interfaces(interface::IInterface& rx_if,
                      interface::IInterface& tx_if) {
    m_tx.set_interface(tx_if);
    m_rx_if_cb = rx_if.add_receive_callback(
        [this](CustomSpan<uint8_t> STR, size_t& SIZE) {
          m_rx.fill(STR, SIZE);
        });
  }
  static constexpr std::chrono::duration RECEIVE_TIMEOUT =
      std::chrono::milliseconds{1000};
  template <typename... Infos>
  auto request(Infos&&... infos) -> RxFieldsSnapshot {
    m_received = false;

    RxFieldsSnapshot result{};
    auto lambda = [&](RxFieldsSnapshot&& snapshot) { result = snapshot; };
    m_inflight_cb = lambda;
    m_tx.send_packet(std::forward<Infos>(infos)...);

    std::unique_lock<std::mutex> lock(m_);
    m_cv.wait_for(lock, RECEIVE_TIMEOUT, [this]() { return m_received; });
    return result;
  }

  template <typename... Infos>
  auto send(Infos&&... infos) -> size_t {
    return m_tx.send_packet(std::forward<Infos>(infos)...);
  }

  void set_receive_callback(
      std::function<void(RxFieldsSnapshot&&)> user_callback) {
    m_user_callback = user_callback;
  }

  RxCont m_rx;
  TxCont m_tx;

 protected:
  // sync
  std::atomic<bool> m_running{true};
  std::function<void(RxFieldsSnapshot&&)> m_user_callback;
  std::thread m_deque_thread;
  std::mutex m_;
  std::mutex m_queue_mutex;
  std::condition_variable m_cv;
  std::condition_variable m_queue_cv;
  bool m_received{false};
  std::deque<RxFieldsSnapshot> m_rx_queue;
  interface::Delegate m_rx_if_cb;
  size_t m_max_deque_size = 100;

  // One-shot extractor installed by wait_once to pull typed DATA_FIELD while Rx
  // buffer is valid.
  std::function<void(RxFieldsSnapshot&&)> m_inflight_cb{};

  // Stored delegates to honor [[nodiscard]] on AddReceiveCallback
  typename RxCont::Delegate m_rx_delegate{};
  typename RxCont::CallbackType m_rx_callback{[this](auto& container) {
    if (container.is_debug()) {
      std::cout << " \n\n Packet is received!!\n" << '\n';
      container.for_each_type([&](auto& field) { field.print(); });
    }
    if (m_inflight_cb) {
      m_inflight_cb(container.get_named_copies());
      m_inflight_cb = nullptr;
    } else {
      std::unique_lock<std::mutex> lock(m_queue_mutex);
      m_rx_queue.emplace_back(container.get_named_copies());
      if (m_rx_queue.size() > m_max_deque_size) {
        // TODO должна быть запись в лог что были потеряны данные
        m_rx_queue.pop_front();
      }
      lock.unlock();
      m_queue_cv.notify_all();
    }
    m_received = true;
    m_cv.notify_all();
  }};
};

}  // namespace proto