#pragma once

#include "LacteObjects.hpp"
#include "ProtocolEndpoint.hpp"
#include "Crc16Modbus.hpp"

namespace proto::lacte {
    template<uint8_t *RX_BASE, uint8_t *TX_BASE>
    class LacteHostProtocol_ : public proto::ProtocolEndpoint<
            typename boardPacket<RX_BASE>::packet_fields,
            typename hostPacket<TX_BASE>::packet_fields,
            Crc16Modbus> {
    public:
      ParamProtocol<RX_BASE> params;
      using ParamsReturnType =  typename ParamProtocol<RX_BASE>::ReceiveType;
      template<typename... Infos>
      ParamsReturnType ReguestParam(Infos&&... infos){
          uint8_t num = GET_PARAMS;
          ParamsReturnType result;
          auto answer = this->Request( proto::MakeFieldInfo<proto::FieldName::TYPE_FIELD>(&num),  std::forward<Infos>(infos)...);
          auto data_field = proto::meta::get_named<proto::FieldName::DATA_FIELD>(answer);
          if(std::holds_alternative<std::vector<uint8_t>>(data_field)){
              std::vector<uint8_t> v = std::get<std::vector<uint8_t>>(data_field);
            CustomSpan<uint8_t> s(v.data(), v.size());
              size_t read = 0;

              auto d = params.rx.AddReceiveCallback([&](auto& container){
                  auto& data_field = container.template Get<proto::FieldName::DATA_FIELD>();
                  result = data_field.GetCopy();
              });
              params.rx.Fill(s, read);

          }
          return result;
      }

     private:

    };

    template<uint8_t *RX_BASE, uint8_t *TX_BASE>
    class LacteBoardProtocol_ : public proto::ProtocolEndpoint<
            typename hostPacket<RX_BASE>::packet_fields,
            typename boardPacket<TX_BASE>::packet_fields,
            Crc16Modbus> {
    public:
        ParamProtocol<RX_BASE> param_protocol;
        using ParamsReturnType =  typename ParamProtocol<RX_BASE>::ReceiveType;

        template<class PARAM_TYPE>
        size_t Answer(Params param_number, PARAM_TYPE  param){
            std::vector<uint8_t> transaction(sizeof(param) + 1);
            transaction[0] = (uint8_t)param_number;
            std::memcpy(transaction.data()+1, (void*)&param, sizeof(param));

            uint8_t num = GET_PARAMS;
            return this->Send(proto::MakeFieldInfo<proto::FieldName::TYPE_FIELD>(&num),
                              proto::MakeFieldInfo<proto::FieldName::DATA_FIELD>(transaction.data(), transaction.size()));
        }

        template<class PARAM_TYPE>
        size_t Answer(PacketNumbers param_number, PARAM_TYPE  param){
            return this->Send(proto::MakeFieldInfo<proto::FieldName::TYPE_FIELD>(&param_number),
                              proto::MakeFieldInfo<proto::FieldName::DATA_FIELD>(&param));
        }

    private:

    };
}