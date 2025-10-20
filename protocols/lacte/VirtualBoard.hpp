#pragma once

#include <cstdint>

#include <libraries/interfaces/Echo.hpp>
#include "ProtocolEndpoint.hpp"

namespace proto::lacte {


    template<uint8_t *RX_BASE, uint8_t *TX_BASE>
    class VirtualBoard{
    public:

        using RxFieldsSnapshot = typename LacteBoardProtocol_<RX_BASE, TX_BASE>::RxFieldsSnapshot;
        static constexpr uint8_t default_magic_word[2] = {0xbe, 0xef};
        static constexpr uint8_t default_lacte_sn[4] = {0x22, 0x34, 0x53, 0x34};
        static constexpr uint8_t default_version[2] = {1, 0};
        static constexpr uint8_t default_prod_date[4] = {0x20, 0x23, 0x10, 0x01};
        static constexpr uint8_t default_mcu_uid[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
        static constexpr uint8_t default_uid_data[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
        static constexpr uint8_t default_machine_sn[12] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
        static constexpr uint8_t default_activation_time[4] = {0x01, 0x05, 0x01, 0x02};
        static constexpr uint8_t default_drink_counter[4] = {0x06, 0x05, 0x04, 0x30};
        static constexpr uint8_t default_time_counter[4] = {0x44, 0x33, 0x22, 0x11};
        static constexpr uint8_t default_reserve[4] = {0x00, 0x00, 0x00, 0x00};
        static constexpr uint8_t default_rfid_id[7] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

        VersionPacketType version_data{1,0};
        InfoPacketType info_data{BoardStatus::IDLE, 0xffff, {}};
        UIDPacketType uid_data{1,0};
        RFIDPacketType rfid{};
        RFIDDataPacketType rfid_data{};
        MagicWord magic_word{};
        LacteSn lacte_sn{};
        ProdDate prod_date{};
        Reserve reserve{};
        McuUid mcu_uid{};
        MachineSn machine_sn{};
        ActivationTime activation_time{};
        DrinkCounter drink_counter{};
        TimeCounter time_counter{};

        // Setter methods for board state fields
        void set_version(const VersionPacketType& v) { version_data = v; }
        void set_info(const InfoPacketType& v) { info_data = v; }
        void set_uid(const UIDPacketType& v) { uid_data = v; }
        void set_rfid(const RFIDPacketType& v) { rfid = v; }
        void set_rfid_data(const RFIDDataPacketType& v) { rfid_data = v; }
        void set_magic_word(const MagicWord& v) { magic_word = v; }
        void set_lacte_sn(const LacteSn& v) { lacte_sn = v; }
        void set_prod_date(const ProdDate& v) { prod_date = v; }
        void set_reserve(const Reserve& v) { reserve = v; }
        void set_mcu_uid(const McuUid& v) { mcu_uid = v; }
        void set_machine_sn(const MachineSn& v) { machine_sn = v; }
        void set_activation_time(const ActivationTime& v) { activation_time = v; }
        void set_drink_counter(const DrinkCounter& v) { drink_counter = v; }
        void set_time_counter(const TimeCounter& v) { time_counter = v; }

        LacteBoardProtocol_<RX_BASE, TX_BASE> board_proto;
        proto::interface::echoInterface from_host_interface{};
        proto::interface::echoInterface from_board_interface{};
        proto::interface::Delegate host_interface_send_delegate;

        proto::interface::Delegate board_interface_send_delegate;
        typename decltype(board_proto.rx)::Delegate board_receive_delegate;

        void SetDebug(bool debug) {
            board_proto.rx.SetDebug(debug);
            board_proto.tx.SetDebug(debug);
        }

        template<class HOST_CONTAINER>
        void SetHost(HOST_CONTAINER& host_container){
            board_interface_send_delegate = from_board_interface.AddReceiveCallback([&](CustomSpan<uint8_t> span, size_t &read){
                host_container.rx.Fill(span, read);
            });
            host_container.SetInterfaces(from_board_interface, from_host_interface);
        }

        explicit VirtualBoard() {
            from_host_interface.Open();
            from_board_interface.Open();

            board_proto.tx.SetInterface(from_board_interface);

            host_interface_send_delegate = from_host_interface.AddReceiveCallback( [this](CustomSpan<uint8_t> span, size_t &read){
                uint8_t data[span.size()];
                memcpy(data, span.data(), span.size());
                board_proto.rx.Fill(span, read);
            });
            board_proto.SetReceiveCallback([&](RxFieldsSnapshot&& snap){
                auto& field   = proto::meta::get_named<proto::FieldName::TYPE_FIELD>(snap);

                if(field == PacketNumbers::INFO){
                    board_proto.Answer(PacketNumbers::INFO, info_data);
                }
                else if(field == PacketNumbers::VERSION){
                    board_proto.Answer(PacketNumbers::VERSION, version_data);
                }

                else if(field == PacketNumbers::UID){
                    board_proto.Answer(PacketNumbers::UID, uid_data);
                }
                else if(field == PacketNumbers::RFID_ID){
                    board_proto.Answer(PacketNumbers::RFID_ID, rfid);
                }
                else if(field == PacketNumbers::RFID_DATA){
                    board_proto.Answer(PacketNumbers::RFID_DATA, rfid_data);
                }
                else if(field == PacketNumbers::RESTART){
                    board_proto.Answer(PacketNumbers::RESTART, proto::EmptyDataType());
                }
                else if(field == PacketNumbers::GET_PARAMS){
                    auto& field_data  = proto::meta::get_named<proto::FieldName::DATA_FIELD>(snap);
                    if(std::holds_alternative<Params>(field_data)){
                        auto& param = std::get<Params>(field_data);
                        switch (param) {
                            case Params::MAGIC_WORD: board_proto.Answer(Params::MAGIC_WORD, magic_word);
                                break;
                            case  Params::LACTE_SN: board_proto.Answer(Params::LACTE_SN, lacte_sn);
                                break;
                            case  Params::PROD_DATE: board_proto.Answer(Params::PROD_DATE, prod_date);
                                break;
                            case  Params::MCU_UID: board_proto.Answer(Params::MCU_UID, mcu_uid);
                                break;
                            case Params::RESERVE: board_proto.Answer(Params::RESERVE, reserve);
                                break;
                            case  Params::MACHINE_SN: board_proto.Answer(Params::MACHINE_SN, machine_sn);
                                break;
                            case  Params::ACTIVATION_TIME: board_proto.Answer(Params::ACTIVATION_TIME, activation_time);
                                break;
                            case  Params::DRINK_COUNTER: board_proto.Answer(Params::DRINK_COUNTER, drink_counter);
                                break;
                            case  Params::TIME_COUNTER: board_proto.Answer(Params::TIME_COUNTER, time_counter);
                                break;

                        }
                    }
                }
            });

            memcpy(magic_word.data, default_magic_word, sizeof(default_magic_word));
            memcpy(lacte_sn.data, default_lacte_sn, sizeof(default_lacte_sn));
            memcpy((uint8_t*)&version_data, default_version, sizeof(default_version));
            memcpy(prod_date.data, default_prod_date, sizeof(default_prod_date));
            memcpy(mcu_uid.data, default_mcu_uid, sizeof(default_mcu_uid));
            memcpy((uint8_t*)&uid_data, default_uid_data, sizeof(default_uid_data));
            memcpy(machine_sn.data, default_machine_sn, sizeof(default_machine_sn));
            memcpy(activation_time.data, default_activation_time, sizeof(default_activation_time));
            memcpy(drink_counter.data, default_drink_counter, sizeof(default_drink_counter));
            memcpy(time_counter.data, default_time_counter, sizeof(default_time_counter));
            memcpy(reserve.data, default_reserve, sizeof(default_reserve));
            memcpy(rfid.id, default_rfid_id, sizeof(default_rfid_id));
            memcpy((uint8_t*)&info_data.rfid, default_rfid_id, sizeof(default_rfid_id));
        }
    };
}



