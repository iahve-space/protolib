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

#include <deque>
#include <functional>

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
class ProtocolNoSysEndpoint {
 public:
  /** @brief RX container type for inbound protocol frames. */
  using RxCont = proto::RxContainer<RxFields, Crc>;
  /** @brief TX container type for outbound protocol frames. */
  using TxCont = proto::TxContainer<TxFields, Crc>;
  using ReceiveType = typename RxCont::ReturnType;
  using RxFieldsSnapshot = typename RxCont::NamedReturnTuple;
  std::deque<RxFieldsSnapshot> m_rx_queue;
  size_t m_max_deque_size = 100;

  /**
   * @brief Construct endpoint and install a permanent RX callback that
   * snapshots the last frame's type code.
   * @note A permanent RX callback is installed that snapshots the last received
   * type code.
   */
  explicit ProtocolNoSysEndpoint(bool debug = false) {
    set_debug(debug);
    // Permanent RX-callback
    // Permanent RX-callback that snapshots only the last type code.
    // NOTE: We intentionally do NOT materialize a variant for DATA_FIELD here,
    // to avoid duplicate-type variant instantiations (e.g., multiple uint8_t*
    // PacketInfo).
    m_rx_delegate = m_rx.add_receive_callback(m_rx_callback);
  }
  ~ProtocolNoSysEndpoint() = default;

  void set_debug(bool VAL) {
    m_rx.set_debug(VAL);
    m_tx.set_debug(VAL);
  }

  template <typename... Infos>
  auto send(Infos&&... infos) -> size_t {
    return m_tx.send_packet(std::forward<Infos>(infos)...);
  }

  void receive(CustomSpan<uint8_t> data) { m_rx.fill(data); }

  void set_receive_callback(
      std::function<void(RxFieldsSnapshot&&)> user_callback) {
    m_user_callback = user_callback;
  }

  RxCont m_rx;
  TxCont m_tx;

 protected:
  std::function<void(RxFieldsSnapshot&&)> m_user_callback;

  // Stored delegates to honor [[nodiscard]] on AddReceiveCallback
  typename RxCont::Delegate m_rx_delegate{};
  typename RxCont::CallbackType m_rx_callback{[this](auto& container) {
    if (container.is_debug()) {
      std::cout << " \n\n Packet is received!!\n" << '\n';
      container.for_each_type([&](auto& field) { field.print(); });
    }
    if (m_user_callback) {
      m_user_callback(container.get_named_copies());
    } else {
      m_rx_queue.emplace_back(container.get_named_copies());
      if (m_rx_queue.size() > m_max_deque_size) {
        // TODO должна быть запись в лог что были потеряны данные
        m_rx_queue.pop_front();
      }
    }
  }};
};

}  // namespace proto