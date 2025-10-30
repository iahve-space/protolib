#pragma once

#include <cstdint>
#include "TestFieldPrototypes.hpp"

namespace proto::test{

#pragma pack(push, 1)
    class dataType{
    public:
        uint8_t u8{8};
        uint16_t u16{16};
        uint32_t u32{32};
        float f{3.14f};
        double d{2.718281828459045};
        [[nodiscard]] bool Equal(const dataType &other) const {
            return u8 == other.u8 && u16 == other.u16 && u32 == other.u32 &&
                   f == other.f && d == other.d;
        }
        bool operator==(const dataType &other) const {
            return Equal(other);
        }
    };
#pragma pack(pop)

#pragma pack(push, 1)
    class dataType2{
    public:
        uint8_t u8{8};
        [[nodiscard]] bool Equal(const dataType2 &other) const {
            return u8 == other.u8;
        }
        bool operator==(const dataType2 &other) const {
            return Equal(other);
        }
    };
#pragma pack(pop)

#pragma pack(push, 1)
    class dataType3{
    public:
        uint16_t u16{16};
        uint32_t u32{32};
        float f{3.14f};
        double d{2.718281828459045};
        [[nodiscard]] bool Equal(const dataType &other) const {
            return  u16 == other.u16 && u32 == other.u32 &&
                    f == other.f && d == other.d;
        }
        bool operator==(const dataType &other) const {
            return Equal(other);
        }
    };
#pragma pack(pop)


    template<uint8_t *BASE, uint8_t *BASE2 = BASE, uint8_t *BASE3 = BASE, uint8_t *BASE4 = BASE, uint8_t *BASE5 = BASE>
    class SympleFields{
    public:
        constexpr static uint8_t prefix[3] = {0xAA, 0xBB, 0xCC};


        using idFieldType = TestFieldPrototype<proto::FieldName::ID_FIELD, const uint8_t *, BASE, proto::FieldFlags::NOTHING, sizeof(SympleFields::prefix), sizeof(SympleFields::prefix), SympleFields::prefix>;
        using lenFieldType = TestFieldPrototype<proto::FieldName::LEN_FIELD, uint8_t, BASE2, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using alenFieldType = TestFieldPrototype<proto::FieldName::ALEN_FIELD, uint8_t, BASE3, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using dataFieldType = TestFieldPrototype<proto::FieldName::DATA_FIELD, dataType, BASE4, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using crcFieldType = TestFieldPrototype<proto::FieldName::CRC_FIELD, uint16_t, BASE5,  proto::FieldFlags::IS_IN_LEN>;
        using proto_fields =
                std::tuple<idFieldType,
                        lenFieldType,
                        alenFieldType,
                        dataFieldType,
                        crcFieldType>;

    };

    template<uint8_t *BASE, uint8_t *BASE2 = BASE, uint8_t *BASE3 = BASE, uint8_t *BASE4 = BASE, uint8_t *BASE5 = BASE, uint8_t *BASE6 = BASE>
    class ComplexFields{
    public:
        constexpr static uint8_t prefix[3] = {0xAA, 0xBB, 0xCC};

        enum Packets : size_t {
            kPacket1 = 1,
            kPacket2 = 2,
            kPacket3 = 3,
            kPacket4 = 4
        };
        using MyPackets = std::tuple<
                proto::PacketInfo<Packets::kPacket1, dataType>,
                proto::PacketInfo<Packets::kPacket2, dataType2>,
                proto::PacketInfo<Packets::kPacket3, dataType3>,
                proto::PacketInfo<Packets::kPacket4, proto::EmptyDataType>
        >;

        using idField2Type = TestFieldPrototype<proto::FieldName::ID_FIELD, const uint8_t*, BASE, proto::FieldFlags::NOTHING, sizeof(ComplexFields::prefix), sizeof(ComplexFields::prefix), ComplexFields::prefix>;
        using lenField2Type = TestFieldPrototype<proto::FieldName::LEN_FIELD, uint8_t, BASE2, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using alenField2Type = TestFieldPrototype<proto::FieldName::ALEN_FIELD, uint8_t, BASE3, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using typeField2Type = TestFieldPrototype<proto::FieldName::TYPE_FIELD, uint8_t, BASE4, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using dataField2Type = TestDataFieldPrototype<MyPackets, BASE5, proto::FieldFlags::IS_IN_LEN>;
        using crcField2Type = TestFieldPrototype<proto::FieldName::CRC_FIELD, uint16_t, BASE6,  proto::FieldFlags::IS_IN_LEN | proto::FieldFlags::REVERSE>;
        using proto_fields =
                std::tuple<idField2Type,
                        lenField2Type,
                        alenField2Type,
                        typeField2Type,
                        dataField2Type,
                        crcField2Type>;

    };
}


