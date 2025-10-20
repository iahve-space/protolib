#pragma once

#include <cstdint>
#include <tuple>
#include <optional>
#include <type_traits>
#include <cstring>
#include <chrono>
#include <ctime>
#include <memory>

#include "ProtocolEndpoint.hpp"

namespace proto::lacte {
    enum PacketNumbers : uint8_t {
        INFO = 0x00,
        VERSION = 0x01,
        UID = 0x02,
        RFID_ID = 0x03,
        RFID_DATA = 0x04,
        SET_PARAMS = 0x40,
        GET_PARAMS = 0x41,
        RESTART = 0x7F
    };

    enum class Params : uint8_t {
        MAGIC_WORD = 0,
        LACTE_SN = 1,
        PROD_DATE = 2,
        RESERVE = 3,
        MCU_UID = 4,
        MACHINE_SN = 5,
        ACTIVATION_TIME = 6,
        DRINK_COUNTER = 7,
        TIME_COUNTER = 8
    };

    enum class BoardStatus: uint8_t {
        IDLE = 0,
        CALIBRATION = 1,
        ERROR = 2,
        READY = 3,
        WORK = 4
    };

    enum class BoardErrors: uint16_t {
        ERROR_FLAG_CALIB_ERROR = 0,
        ERROR_FLAG_MOTOR_ERROR = 1,
        ERROR_FLAG_RFID_ERROR = 2,
        ERROR_FLAG_RFID_NO_CARD = 3,
        ERROR_FLAG_RFID_BAD_CARD = 4
    };

#pragma pack(push, 1)

    struct RFIDPacketType{
        uint8_t id[7];
        bool operator==(const RFIDPacketType& other) const {
            return std::memcmp(id, other.id, sizeof(id)) == 0;
        }
    };
    struct InfoPacketType{
        BoardStatus status;
        uint16_t errors;
        RFIDPacketType rfid;
        [[nodiscard]] bool IsCalibrationError() const{
            return ((errors >> (int)BoardErrors::ERROR_FLAG_CALIB_ERROR) & 1);
        }

        [[nodiscard]] bool CheckError(BoardErrors offset) const{
            return ((errors >> (int)offset) & 1);
        }

        bool operator==(const InfoPacketType& other) const{
            return status == other.status && errors == other.errors;
        }
        bool operator!=(const InfoPacketType& other) const{
            return status != other.status || errors != other.errors;
        }
    };
    struct VersionPacketType{
        uint8_t major;
        uint8_t minor;
        bool operator==(const VersionPacketType& other) const{
            return major == other.major && minor == other.minor;
        }
    };
    struct UIDPacketType{
        uint8_t uid[12];
        bool operator==(const UIDPacketType& other) const {
            return std::memcmp(uid, other.uid, sizeof(uid)) == 0;
        }
    };

    struct RFIDDataPacketType{
        uint8_t data[48];
        bool operator==(const RFIDDataPacketType& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct MagicWord{
        uint8_t data[2]{};
        bool operator==(const MagicWord& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct LacteSn{
        uint8_t data[6]{};
        bool operator==(const LacteSn& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct ProdDate{
        uint8_t data[4]{};
        bool operator==(const ProdDate& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct Reserve{
        uint8_t data[4]{};
        bool operator==(const Reserve& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct McuUid{
        uint8_t data[12]{};
        bool operator==(const McuUid& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct MachineSn{
        uint8_t data[4]{};
        bool operator==(const MachineSn& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct ActivationTime{
        uint8_t data[4]{};
        bool operator==(const ActivationTime& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct DrinkCounter{
        uint8_t data[4]{};
        bool operator==(const DrinkCounter& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };
    struct TimeCounter{
        uint8_t data[4]{};
        bool operator==(const TimeCounter& other) const {
            return std::memcmp(data, other.data, sizeof(data)) == 0;
        }
    };

#pragma pack(pop)



    template<uint8_t* BASE>
    struct ParamPacket{

        using Packets = std::tuple<
                proto::PacketInfo<(size_t)Params::MAGIC_WORD, MagicWord>,
                proto::PacketInfo<(size_t)Params::LACTE_SN, LacteSn>,
                proto::PacketInfo<(size_t)Params::PROD_DATE, ProdDate>,
                proto::PacketInfo<(size_t)Params::RESERVE, Reserve>,
                proto::PacketInfo<(size_t)Params::MCU_UID, McuUid>,
                proto::PacketInfo<(size_t)Params::MACHINE_SN, MachineSn>,
                proto::PacketInfo<(size_t)Params::ACTIVATION_TIME, ActivationTime>,
                proto::PacketInfo<(size_t)Params::DRINK_COUNTER, DrinkCounter>,
                proto::PacketInfo<(size_t)Params::TIME_COUNTER, TimeCounter>
        >;
        using typeFieldType = proto::FieldPrototype< proto::FieldName::TYPE_FIELD, uint8_t, BASE, proto::FieldFlags::NOTHING>;
        using dataFieldType = proto::DataFieldPrototype< Packets,BASE, proto::FieldFlags::NOTHING>;

        using packet_fields = std::tuple<
                typeFieldType,
                dataFieldType
        >;
    };

    template<uint8_t *BASE>
    class ParamProtocol : public proto::ProtocolEndpoint<
            typename ParamPacket<BASE>::packet_fields,
            typename ParamPacket<BASE>::packet_fields> {
    public:
    };

    template<uint8_t* BASE>
    struct hostPacket{
        constexpr static uint8_t prefix[2] = {0xFF,0x55};

        using Packets = std::tuple<
                proto::PacketInfo<INFO, proto::EmptyDataType>,
                proto::PacketInfo<VERSION, proto::EmptyDataType>,
                proto::PacketInfo<UID, proto::EmptyDataType>,
                proto::PacketInfo<RFID_ID, proto::EmptyDataType>,
                proto::PacketInfo<RFID_DATA, proto::EmptyDataType>,
                proto::PacketInfo<SET_PARAMS, uint8_t*>,
                proto::PacketInfo<GET_PARAMS, Params>,
                proto::PacketInfo<RESTART, uint8_t*>
        >;

        using idFieldType = proto::FieldPrototype<proto::FieldName::ID_FIELD, const uint8_t*, BASE, proto::FieldFlags::NOTHING, 2, 2, hostPacket::prefix>;
        using lenFieldType = proto::FieldPrototype<proto::FieldName::LEN_FIELD, uint8_t, BASE, proto::FieldFlags::IS_IN_CRC>;
        using timeFieldType = proto::FieldPrototype<proto::FieldName::TIME_FIELD, uint32_t , BASE, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using typeFieldType = proto::FieldPrototype<proto::FieldName::TYPE_FIELD, uint8_t , BASE, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using dataFieldType = proto::DataFieldPrototype< Packets,BASE, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using crcFieldType = proto::FieldPrototype<proto::FieldName::CRC_FIELD, uint16_t, BASE,  proto::FieldFlags::REVERSE>;

        using packet_fields = std::tuple<
                idFieldType,
                lenFieldType,
                timeFieldType,
                typeFieldType,
                dataFieldType,
                crcFieldType
        >;
    };

    template<uint8_t* BASE>
    struct boardPacket {
        constexpr static uint8_t prefix[2] = {0xFF,0xAA};

        using Packets = std::tuple<
                proto::PacketInfo<INFO, InfoPacketType>,
                proto::PacketInfo<VERSION, VersionPacketType>,
                proto::PacketInfo<UID, UIDPacketType>,
                proto::PacketInfo<RFID_ID, RFIDPacketType>,
                proto::PacketInfo<RFID_DATA, RFIDDataPacketType>,
                proto::PacketInfo<SET_PARAMS, uint8_t*>,
                proto::PacketInfo<GET_PARAMS, uint8_t*>,
                proto::PacketInfo<RESTART, proto::EmptyDataType>
        >;
        using boardIdFieldType = proto::FieldPrototype<proto::FieldName::ID_FIELD, const uint8_t*, BASE, proto::FieldFlags::NOTHING, 2, 2, boardPacket::prefix>;
        using boardLenFieldType = proto::FieldPrototype<proto::FieldName::LEN_FIELD, uint8_t, BASE, proto::FieldFlags::IS_IN_CRC>;
        using boardAnsCommFieldType = proto::FieldPrototype<proto::FieldName::TYPE_FIELD, uint8_t , BASE, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using boardDataFieldType = proto::DataFieldPrototype<Packets, BASE, proto::FieldFlags::IS_IN_CRC | proto::FieldFlags::IS_IN_LEN>;
        using boardcrcFieldType = proto::FieldPrototype<proto::FieldName::CRC_FIELD, uint16_t, BASE,  proto::FieldFlags::REVERSE>;

        using packet_fields = std::tuple<
                boardIdFieldType,
                boardLenFieldType,
                boardAnsCommFieldType,
                boardDataFieldType,
                boardcrcFieldType
        >;
    };

    static inline std::istream& operator>>(std::istream& is, Params& num) {
        size_t n;
        is >> n;
        if (n > (size_t)Params::TIME_COUNTER) {
            is.setstate(std::ios::failbit);
        } else {
            num = static_cast<Params>(n);
        }
        return is;
    }
    inline void print_bytes(std::ostream& os, const uint8_t* data, std::size_t n) {
        if (!data || n == 0) return;

        std::ios old(nullptr);
        old.copyfmt(os);              // сохранить формат
        auto old_fill = os.fill();

        os << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < n; ++i) {
            if (i) os << ' ';         // пробел только между байтами
            os << std::setw(2) << static_cast<unsigned>(data[i]);
        }

        os.fill(old_fill);
        os.copyfmt(old);              // восстановить формат
        os << std::endl;
    }
    inline std::ostream& operator<<(std::ostream& os, const MagicWord& v) {
        os << "\nMagicWord: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const LacteSn& v) {
        os << "\nLacteSn: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const ProdDate& v) {
        os << "\nProdDate: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const McuUid& v) {
        os << "\nMcuUid: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const MachineSn& v) {
        os << "\nMachineSn: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const ActivationTime& v) {
        os << "\nActivationTime: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const DrinkCounter& v) {
        os << "\nDrinkCounter: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const TimeCounter& v) {
        os << "\nTimeCounter: ";
        print_bytes(os, v.data, sizeof v.data);
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const InfoPacketType& v) {
        os << "\nInfoPacket: status=" << static_cast<int>(v.status) << "\nErrors=0x" << std::hex << v.errors << std::endl;
        os << "RFID ID: ";
        print_bytes(os, v.rfid.id, sizeof(v.rfid.id));
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const VersionPacketType& v) {
        os << "\nVersionPacket: major=" << static_cast<int>(v.major)
           << ", minor=" << static_cast<int>(v.minor) << std::endl;
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const UIDPacketType& v) {
        os << "\nUIDPacket: ";
        print_bytes(os, v.uid, sizeof(v.uid));
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const RFIDPacketType& v) {
        os << "\nRFIDPacket: ";
        print_bytes(os, v.id, sizeof(v.id));
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const RFIDDataPacketType& v) {
        os << "\nRFIDDataPacket: ";
        print_bytes(os, v.data, sizeof(v.data));
        return os;
    }
    inline std::ostream& operator<<(std::ostream& os, const proto::EmptyDataType&) {
        os << "\nEmptyDataType" << std::endl;
        return os;
    }

}