#include <gtest/gtest.h>

#include "LacteProtocol.hpp"
#include <libraries/interfaces/Echo.hpp>
#include "VirtualBoard.hpp"

using namespace proto;
namespace proto::lacte::Tests{
    uint8_t host_rx_buffer[300];
    uint8_t host_tx_buffer[300];
    uint8_t board_rx_buffer[300];
    uint8_t board_tx_buffer[300];

    using virtual_board = VirtualBoard<board_rx_buffer, board_tx_buffer>;

    TEST(LacteProtocolTest, Main)
    {
        LacteHostProtocol_<host_rx_buffer, host_tx_buffer> host_proto;
        virtual_board board{};
        board.SetDebug(true);

        host_proto.SetInterfaces(board.from_board_interface, board.from_host_interface);
        host_proto.SetDebug(true);

        uint8_t test_buffer[100] = {0xff, 0x00};
        int read = 0;
      CustomSpan<uint8_t> test_span(test_buffer, sizeof(test_buffer));
        board.from_host_interface.Write(test_span, std::chrono::milliseconds{1000});

        uint8_t buff[] = {0xff, 0xaa, 0x0d, 0x02, 0x32, 0xff, 0xd8, 0x05, 0x47, 0x50, 0x35, 0x32, 0x30, 0x64, 0x24, 0x57, 0x9e, 0xad};
      CustomSpan<uint8_t> test_span2(buff, sizeof(buff));
        board.from_board_interface.Write(test_span2, std::chrono::milliseconds{1000});

        auto request = [&](auto type, auto&& answer_type)->void{
            auto answer = host_proto.Request(MakeFieldInfo<FieldName::TYPE_FIELD>(&type));
            auto& data_field = meta::get_named<proto::FieldName::DATA_FIELD>(answer);
            ASSERT_TRUE(std::holds_alternative<std::remove_reference_t<decltype(answer_type)>>(data_field));
            EXPECT_EQ(std::get<std::remove_reference_t<decltype(answer_type)>>(data_field), answer_type);
        };
        // --- обычные команды ---
        request(proto::lacte::INFO, board.info_data);
        request(proto::lacte::VERSION, board.version_data);
        request(proto::lacte::UID, board.uid_data);
        request(proto::lacte::RFID_ID, board.rfid);
        request(proto::lacte::RFID_DATA, board.rfid_data);
        request(proto::lacte::RESTART, proto::EmptyDataType{});

        auto request_param = [&](auto type, auto&& answer_type)->void{
            auto answer = host_proto.ReguestParam(MakeFieldInfo<FieldName::DATA_FIELD>(&type));
            ASSERT_TRUE(std::holds_alternative<std::remove_reference_t<decltype(answer_type)>>(answer));
            EXPECT_EQ(std::get<std::remove_reference_t<decltype(answer_type)>>(answer), answer_type);
        };
        // --- параметры (GET_PARAMS) ---
        request_param(proto::lacte::Params::LACTE_SN, board.lacte_sn);
        request_param(proto::lacte::Params::MAGIC_WORD, board.magic_word);
        request_param(proto::lacte::Params::PROD_DATE, board.prod_date);
        request_param(proto::lacte::Params::MCU_UID, board.mcu_uid);
        request_param(proto::lacte::Params::MACHINE_SN, board.machine_sn);
        request_param(proto::lacte::Params::ACTIVATION_TIME, board.activation_time);
        request_param(proto::lacte::Params::DRINK_COUNTER, board.drink_counter);
        request_param(proto::lacte::Params::TIME_COUNTER, board.time_counter);
    }

}
