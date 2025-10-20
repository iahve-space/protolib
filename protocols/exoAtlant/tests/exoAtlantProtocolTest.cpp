#include <gtest/gtest.h>

#include "exoAtlantProtocol.hpp"
#include <libraries/interfaces/Echo.hpp>

using namespace proto::exoAtlant;
namespace proto::lacte::Tests{
    class ScopeTimer {
    public:
        explicit ScopeTimer(const char* name)
            : name_(name),
              start_(std::chrono::high_resolution_clock::now())
        {}

        ~ScopeTimer() {
            using namespace std::chrono;
            auto end = high_resolution_clock::now();
            auto elapsed_us = duration_cast<microseconds>(end - start_).count();
            printf("\n[%s] took %lld us (%.3f ms)\n",
                   name_,
                   static_cast<long long>(elapsed_us),
                   elapsed_us / 1000.0);
        }

    private:
        const char* name_;
        std::chrono::high_resolution_clock::time_point start_;
    };


    using namespace proto::exoAtlant;
    uint8_t host_rx_buffer[300];
    uint8_t host_tx_buffer[300];
    uint8_t board_rx_buffer[300];
    uint8_t board_tx_buffer[300];

    template<typename T>
    pkt_desc_t createPacket(T& data)
    {
        pkt_desc_t packet{};
        packet.ver =3;
        packet.type = PACKET1;
        packet.size = sizeof(T);
        packet.addr_src = 33;
        packet.addr_dst = 34;
        packet.data = (uint8_t*)&data;
        return packet;
    }
    uint8_t buf[100]{};

    TEST(exoatlantProtocolTest, Main)
    {
        //debug включает подробный вывод парсера но отнимает значительное время на печать.
        ExoAtlantProtocol_<host_rx_buffer, host_tx_buffer> host_proto(false);

        Packet1 packet1_content{.number = 33, .data = {1,2,5,2,3,4}};
        auto packet = createPacket(packet1_content);

        uint result_size{};

        //добавлено для фрагментации данных. Так же можно отправлять фрагменты рандомных размеров
        {
            ScopeTimer scope("serialize");
            result_size = host_proto.serialize(packet,buf , sizeof(buf));
        }

        pkt_desc_t answer{};
        {
            ScopeTimer scope("deserialize");
            for (uint i = 0; i < result_size - 1; i++ )
            {
                host_proto.parse(buf+ i, 1);
            }
            answer = host_proto.parse(buf+ result_size - 1, 1);
        }
        ASSERT_EQ(packet, answer);
        host_proto.reset();
    }

}
