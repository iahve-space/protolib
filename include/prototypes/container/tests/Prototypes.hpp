#pragma once

#include "ProtocolEndpoint.hpp"
#include "TestFields.hpp"

namespace proto::test {

template <uint8_t *RX_BASE, uint8_t *TX_BASE>
class SympleProtocol
    : public ProtocolEndpoint<typename SympleFields<RX_BASE>::proto_fields,
                              typename SympleFields<TX_BASE>::proto_fields> {};

template <uint8_t *RX_BASE, uint8_t *TX_BASE>
class ComplexProtocol
    : public ProtocolEndpoint<typename ComplexFields<RX_BASE>::proto_fields,
                              typename ComplexFields<TX_BASE>::proto_fields> {};
}  // namespace proto::test
