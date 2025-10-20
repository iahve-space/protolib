/**
 * @file ProtocolEndpoint.hpp
 * @brief Base endpoint class for protocol implementations.
 *
 * This header defines a generic ProtocolEndpoint template for managing protocol
 * communication endpoints. It provides containers for RX and TX fields, a snapshot
 * mechanism for the last received frame, and thread-safe access to recent data.
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

#include <memory>
#include <functional>
#include <deque>

#include "prototypes/container/RxContainer.hpp"
#include "prototypes/container/TxContainer.hpp"

namespace proto{

/**
 * @brief Protocol endpoint base for protocol implementations.
 *
 * @tparam RxFields Protocol RX field set.
 * @tparam TxFields Protocol TX field set.
 * @tparam Crc      CRC type for integrity.
 *
 * Responsibilities:
 *   - Handles RX/TX containers.
 *   - Snapshots the last received frame's type code (payload can be extracted on-demand via wait_once).
 *   - Provides thread-safe access and waiting.
 *   - Allows user RX callback hooks.
 *
 * Lifecycle:
 *   - Construct, wire interfaces, use send/wait methods, query snapshot.
 */
    template<class RxFields, class TxFields, class Crc = CrcSoft>
    class ProtocolNoSysEndpoint {
    public:
        /** @brief RX container type for inbound protocol frames. */
        using RxCont = proto::RxContainer<RxFields, Crc>;
        /** @brief TX container type for outbound protocol frames. */
        using TxCont = proto::TxContainer<TxFields, Crc>;
        using ReceiveType = typename RxCont::ReturnType;
        using RxFieldsSnapshot = typename RxCont::NamedReturnTuple;
        std::deque<RxFieldsSnapshot> rx_queue_;
        size_t max_deque_size_ = 100;

        /**
         * @brief Construct endpoint and install a permanent RX callback that snapshots the last frame's type code.
         * @note A permanent RX callback is installed that snapshots the last received type code.
         */
        explicit ProtocolNoSysEndpoint(bool debug=false) {
            SetDebug(debug);
            // Permanent RX-callback
            // Permanent RX-callback that snapshots only the last type code.
            // NOTE: We intentionally do NOT materialize a variant for DATA_FIELD here,
            // to avoid duplicate-type variant instantiations (e.g., multiple uint8_t* PacketInfo).
            rx_delegate_ = rx.AddReceiveCallback(rx_callback);
        }
        ~ProtocolNoSysEndpoint(){}

        void SetDebug(bool v){ rx.SetDebug(v); tx.SetDebug(v); }

        template<typename... Infos>
        size_t Send( Infos&&... infos) {
            return tx.SendPacket(std::forward<Infos>(infos)...);
        }

        void Receive(CustomSpan<uint8_t> data)
        {
            rx.Fill(data);
        }

        void SetReceiveCallback(std::function<void(RxFieldsSnapshot&&)> user_callback){
            user_callback_ = user_callback;
        }

        RxCont rx;
        TxCont tx;

    protected:

        std::function<void(RxFieldsSnapshot&&)> user_callback_;

        // Stored delegates to honor [[nodiscard]] on AddReceiveCallback
        typename RxCont::Delegate rx_delegate_{};
        typename RxCont::CallbackType rx_callback{[this](auto& container){
            if(container.IsDebug()){
                std::cout << " \n\n Packet is received!!\n" <<std::endl;
                container.for_each_type([&](auto& field){
                    field.Print();
                });
            }
            if (user_callback_)
            {
                user_callback_(container.GetNamedCopies());
            }
            else
            {
                rx_queue_.emplace_back(container.GetNamedCopies());
                if(rx_queue_.size() > max_deque_size_){
                    //TODO должна быть запись в лог что были потеряны данные
                    rx_queue_.pop_front();
                }
            }
        }};

    };

}