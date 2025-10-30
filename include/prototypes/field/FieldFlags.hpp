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

#include <array>
#include <cstdint>
#include <ostream>
#include <string_view>

using namespace std::string_view_literals;

namespace proto {

/**
 * @enum FieldName
 * @brief Identifiers for standard protocol fields.
 *
 * Used as compile-time tags for packet generation and parsing.
 */
enum class FieldName : uint8_t {
  ID_FIELD [[maybe_unused]],        //!< Identifier field.
  ID_2_FIELD [[maybe_unused]],      //!< Secondary identifier.
  TYPE_FIELD [[maybe_unused]],      //!< Message type.
  REQ_TYPE_FIELD [[maybe_unused]],  //!< Request type.
  ANS_TYPE_FIELD [[maybe_unused]],  //!< Response type.
  LEN_FIELD [[maybe_unused]],       //!< Payload length.
  ALEN_FIELD [[maybe_unused]],      //!< Alternative length / extended length.
  SOURCE_FIELD [[maybe_unused]],    //!< Source identifier.
  DEST_FIELD [[maybe_unused]],      //!< Destination identifier.
  VERSION_FIELD [[maybe_unused]],   //!< Protocol or format version.
  NUMBER_FIELD [[maybe_unused]],    //!< Sequence number / counter.
  DATA_FIELD [[maybe_unused]],      //!< Payload data.
  CRC_FIELD [[maybe_unused]],       //!< CRC checksum.
  SESSION_FIELD [[maybe_unused]],   //!< Session identifier.
  DUMP_FIELD [[maybe_unused]],      //!< Debug / raw dump field.
  HEADER_FIELD [[maybe_unused]],    //!< Message header.
  BIN_FIELD [[maybe_unused]],       //!< Binary blob.
  TIME_FIELD [[maybe_unused]],      //!< Timestamp.
  HEIGHT_FIELD [[maybe_unused]],    //!< Height value / vertical dimension.
  WIDTH_FIELD [[maybe_unused]],     //!< Width value / horizontal dimension.
  STATUS_FIELD [[maybe_unused]],    //!< Status / state code.
};

/**
 * @brief Table of FieldName to string mappings.
 *
 * Used by @ref ToString for human-readable names.
 */
static constexpr std::array<std::pair<FieldName, std::string_view>, 21>
    K_FIELD_NAME_STRINGS{{
        {FieldName::ID_FIELD, "ID_FIELD"sv},
        {FieldName::ID_2_FIELD, "ID_2_FIELD"sv},
        {FieldName::TYPE_FIELD, "TYPE_FIELD"sv},
        {FieldName::REQ_TYPE_FIELD, "REQ_TYPE_FIELD"sv},
        {FieldName::ANS_TYPE_FIELD, "ANS_TYPE_FIELD"sv},
        {FieldName::LEN_FIELD, "LEN_FIELD"sv},
        {FieldName::ALEN_FIELD, "ALEN_FIELD"sv},
        {FieldName::SOURCE_FIELD, "SOURCE_FIELD"sv},
        {FieldName::DEST_FIELD, "DEST_FIELD"sv},
        {FieldName::VERSION_FIELD, "VERSION_FIELD"sv},
        {FieldName::NUMBER_FIELD, "NUMBER_FIELD"sv},
        {FieldName::DATA_FIELD, "DATA_FIELD"sv},
        {FieldName::CRC_FIELD, "CRC_FIELD"sv},
        {FieldName::SESSION_FIELD, "SESSION_FIELD"sv},
        {FieldName::DUMP_FIELD, "DUMP_FIELD"sv},
        {FieldName::HEADER_FIELD, "HEADER_FIELD"sv},
        {FieldName::BIN_FIELD, "BIN_FIELD"sv},
        {FieldName::TIME_FIELD, "TIME_FIELD"sv},
        {FieldName::HEIGHT_FIELD, "HEIGHT_FIELD"sv},
        {FieldName::WIDTH_FIELD, "WIDTH_FIELD"sv},
        {FieldName::STATUS_FIELD, "STATUS_FIELD"sv},
    }};

/**
 * @brief Convert FieldName enum to string.
 * @param NAME FieldName value.
 * @return Corresponding string, or `"UNKNOWN"` if not found.
 */
constexpr auto to_string(const FieldName NAME) -> std::string_view {
  for (const auto& [kEy, kStr] : K_FIELD_NAME_STRINGS) {
    if (kEy == NAME) {
      return kStr;
    }
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
enum class FieldFlags : uint8_t {
  NOTHING = 0,         //!< No flags set.
  IS_IN_LEN = 1,       //!< Field contributes to length calculation.
  IS_IN_CRC = 1 << 1,  //!< Field contributes to CRC calculation.
  REVERSE = 1 << 2,    //!< Field bytes are stored in reverse order.
  SUPPRESS = 1 << 3,   //!< Field should be suppressed (hidden/skipped).
  CONST_SIZE = 1 << 4  //!< Field has a constant size.
};

/// Bitwise OR operator for FieldFlags.
constexpr auto operator|(const FieldFlags FIRST, const FieldFlags SECOND)
    -> FieldFlags {
  return static_cast<FieldFlags>(static_cast<uint64_t>(FIRST) |
                                 static_cast<uint64_t>(SECOND));
}

/// Bitwise AND operator for FieldFlags.
constexpr auto operator&(const FieldFlags FIRST, const FieldFlags SECOND)
    -> FieldFlags {
  return static_cast<FieldFlags>(static_cast<uint64_t>(FIRST) &
                                 static_cast<uint64_t>(SECOND));
}

/// Bitwise NOT operator for FieldFlags.
constexpr auto operator~(const FieldFlags VAL) -> FieldFlags {
  return static_cast<FieldFlags>(~static_cast<uint64_t>(VAL));
}

/// Logical negation: true if no flags are set.
constexpr auto operator!(const FieldFlags FLAG) -> bool {
  return static_cast<uint64_t>(FLAG) == 0;
}

/**
 * @brief Generic flag check helper.
 *
 * Works with any enum type supporting `&`.
 *
 * @tparam Enum Enum type (usually @ref FieldFlags).
 * @param VALUE Combined flag value.
 * @param FLAG Flag to check.
 * @return true if the flag is set.
 */
template <typename Enum>
constexpr auto has_flag(const Enum VALUE, const Enum FLAG) -> bool {
  return static_cast<uint64_t>(VALUE & FLAG) != 0;
}

/**
 * @brief Stream output for FieldFlags.
 *
 * Format: joined list of flag names separated by `|`.
 * For empty flags prints `"NOTHING"`.
 *
 * @param out Output stream.
 * @param FLAGS FieldFlags value.
 * @return Reference to output stream.
 */
inline auto operator<<(std::ostream& out, const FieldFlags FLAGS)
    -> std::ostream& {
  if (FLAGS == FieldFlags::NOTHING) {
    return out << "NOTHING"sv;
  }

  bool first = true;
  auto append = [&](const char* name) {
    if (!first) {
      out << "|";
    }
    out << name;
    first = false;
  };

  if (has_flag(FLAGS, FieldFlags::IS_IN_LEN)) {
    append("IS_IN_LEN");
  }
  if (has_flag(FLAGS, FieldFlags::IS_IN_CRC)) {
    append("IS_IN_CRC");
  }
  if (has_flag(FLAGS, FieldFlags::REVERSE)) {
    append("REVERSE");
  }
  if (has_flag(FLAGS, FieldFlags::SUPPRESS)) {
    append("SUPPRESS");
  }
  if (has_flag(FLAGS, FieldFlags::CONST_SIZE)) {
    append("CONST_SIZE");
  }
  return out;
}

}  // namespace proto