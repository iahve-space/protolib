#pragma once
/**
 * @file FieldFlags.hpp
 * @brief Common enums and utilities for protocol field names and flags.
 *
 * Provides:
 *  - @ref proto::FieldName — enumeration of standard field identifiers;
 *  - @ref proto::ToString — conversion of enum to string;
 *  - @ref proto::FieldFlags — bitwise flags describing field properties;
 *  - bitwise operators for @ref proto::FieldFlags and stream output.
 */

#include <cstdint>
#include <cstring>
#include <sstream>
#include <typeindex>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <array>
#include <string_view>
#include <limits>

using namespace std::string_view_literals;
using namespace std::string_literals;

namespace proto {

    /**
     * @enum FieldName
     * @brief Identifiers for standard protocol fields.
     *
     * Used as compile-time tags for packet generation and parsing.
     */
    enum class FieldName : uint64_t
    {
        ID_FIELD         [[maybe_unused]], //!< Identifier field.
        ID_2_FIELD       [[maybe_unused]], //!< Secondary identifier.
        TYPE_FIELD       [[maybe_unused]], //!< Message type.
        REQ_TYPE_FIELD   [[maybe_unused]], //!< Request type.
        ANS_TYPE_FIELD   [[maybe_unused]], //!< Response type.
        LEN_FIELD        [[maybe_unused]], //!< Payload length.
        ALEN_FIELD       [[maybe_unused]], //!< Alternative length / extended length.
        SOURCE_FIELD     [[maybe_unused]], //!< Source identifier.
        DEST_FIELD       [[maybe_unused]], //!< Destination identifier.
        VERSION_FIELD    [[maybe_unused]], //!< Protocol or format version.
        NUMBER_FIELD     [[maybe_unused]], //!< Sequence number / counter.
        DATA_FIELD       [[maybe_unused]], //!< Payload data.
        CRC_FIELD        [[maybe_unused]], //!< CRC checksum.
        SESSION_FIELD    [[maybe_unused]], //!< Session identifier.
        DUMP_FIELD       [[maybe_unused]], //!< Debug / raw dump field.
        HEADER_FIELD     [[maybe_unused]], //!< Message header.
        BIN_FIELD        [[maybe_unused]], //!< Binary blob.
        TIME_FIELD       [[maybe_unused]], //!< Timestamp.
        HEIGHT_FIELD     [[maybe_unused]], //!< Height value / vertical dimension.
        WIDTH_FIELD      [[maybe_unused]], //!< Width value / horizontal dimension.
        STATUS_FIELD     [[maybe_unused]], //!< Status / state code.
    };

    /**
     * @brief Table of FieldName to string mappings.
     *
     * Used by @ref ToString for human-readable names.
     */
    static constexpr std::array<std::pair<FieldName, std::string_view>, 21> kFieldNameStrings{{
                                                                                                      {FieldName::ID_FIELD,       "ID_FIELD"sv},
                                                                                                      {FieldName::ID_2_FIELD,     "ID_2_FIELD"sv},
                                                                                                      {FieldName::TYPE_FIELD,     "TYPE_FIELD"sv},
                                                                                                      {FieldName::REQ_TYPE_FIELD, "REQ_TYPE_FIELD"sv},
                                                                                                      {FieldName::ANS_TYPE_FIELD, "ANS_TYPE_FIELD"sv},
                                                                                                      {FieldName::LEN_FIELD,      "LEN_FIELD"sv},
                                                                                                      {FieldName::ALEN_FIELD,     "ALEN_FIELD"sv},
                                                                                                      {FieldName::SOURCE_FIELD,   "SOURCE_FIELD"sv},
                                                                                                      {FieldName::DEST_FIELD,     "DEST_FIELD"sv},
                                                                                                      {FieldName::VERSION_FIELD,  "VERSION_FIELD"sv},
                                                                                                      {FieldName::NUMBER_FIELD,   "NUMBER_FIELD"sv},
                                                                                                      {FieldName::DATA_FIELD,     "DATA_FIELD"sv},
                                                                                                      {FieldName::CRC_FIELD,      "CRC_FIELD"sv},
                                                                                                      {FieldName::SESSION_FIELD,  "SESSION_FIELD"sv},
                                                                                                      {FieldName::DUMP_FIELD,     "DUMP_FIELD"sv},
                                                                                                      {FieldName::HEADER_FIELD,   "HEADER_FIELD"sv},
                                                                                                      {FieldName::BIN_FIELD,      "BIN_FIELD"sv},
                                                                                                      {FieldName::TIME_FIELD,     "TIME_FIELD"sv},
                                                                                                      {FieldName::HEIGHT_FIELD,   "HEIGHT_FIELD"sv},
                                                                                                      {FieldName::WIDTH_FIELD,    "WIDTH_FIELD"sv},
                                                                                                      {FieldName::STATUS_FIELD,   "STATUS_FIELD"sv},
                                                                                              }};

    /**
     * @brief Convert FieldName enum to string.
     * @param name FieldName value.
     * @return Corresponding string, or `"UNKNOWN"` if not found.
     */
    constexpr std::string_view ToString(FieldName name) {
        for (const auto& [kEy, kStr] : kFieldNameStrings) {
            if (kEy == name)
                return kStr;
        }
        return "UNKNOWN"sv;
    }

    /**
     * @enum FieldFlags
     * @brief Bit flags describing protocol field properties.
     *
     * Flags can be combined using bitwise operators.
     * Use @ref HasFlag for checks.
     */
    enum class FieldFlags : uint64_t {
        NOTHING    = 0,        //!< No flags set.
        IS_IN_LEN  = 1,        //!< Field contributes to length calculation.
        IS_IN_CRC  = 1 << 1,   //!< Field contributes to CRC calculation.
        REVERSE    = 1 << 2,   //!< Field bytes are stored in reverse order.
        SUPPRESS   = 1 << 3,   //!< Field should be suppressed (hidden/skipped).
        CONST_SIZE = 1 << 4    //!< Field has a constant size.
    };

    /// Bitwise OR operator for FieldFlags.
    constexpr FieldFlags operator|(FieldFlags a, FieldFlags b) {
        return static_cast<FieldFlags>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    /// Bitwise AND operator for FieldFlags.
    constexpr FieldFlags operator&(FieldFlags a, FieldFlags b) {
        return static_cast<FieldFlags>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
    }

    /// Bitwise NOT operator for FieldFlags.
    constexpr FieldFlags operator~(FieldFlags a) {
        return static_cast<FieldFlags>(~static_cast<uint64_t>(a));
    }

    /// Logical negation: true if no flags are set.
    constexpr bool operator!(FieldFlags f) {
        return static_cast<uint64_t>(f) == 0;
    }

    /**
     * @brief Generic flag check helper.
     *
     * Works with any enum type supporting `&`.
     *
     * @tparam Enum Enum type (usually @ref FieldFlags).
     * @param value Combined flag value.
     * @param flag Flag to check.
     * @return true if the flag is set.
     */
    template<typename Enum>
    constexpr bool HasFlag(Enum value, Enum flag) {
        return static_cast<uint64_t>(value & flag) != 0;
    }

    /**
     * @brief Stream output for FieldFlags.
     *
     * Format: joined list of flag names separated by `|`.
     * For empty flags prints `"NOTHING"`.
     *
     * @param os Output stream.
     * @param flags FieldFlags value.
     * @return Reference to output stream.
     */
    inline std::ostream& operator<<(std::ostream& os, FieldFlags flags) {
        if (flags == FieldFlags::NOTHING) return os << "NOTHING"sv;

        bool first = true;
        auto append = [&](const char* name) {
            if (!first) os << "|";
            os << name;
            first = false;
        };

        if (HasFlag(flags, FieldFlags::IS_IN_LEN))   append("IS_IN_LEN");
        if (HasFlag(flags, FieldFlags::IS_IN_CRC))   append("IS_IN_CRC");
        if (HasFlag(flags, FieldFlags::REVERSE))     append("REVERSE");
        if (HasFlag(flags, FieldFlags::SUPPRESS))    append("SUPPRESS");
        if (HasFlag(flags, FieldFlags::CONST_SIZE))  append("CONST_SIZE");

        return os;
    }

} // namespace proto