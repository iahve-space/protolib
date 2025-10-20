#pragma once

#include "ProtocolEndpoint.hpp"
#include "TestFieldPrototypes.hpp"
#include "TestFields.hpp"

namespace proto::test{

    template<uint8_t *RX_BASE, uint8_t *TX_BASE>
    class SympleProtocol: public ::proto::ProtocolEndpoint<typename SympleFields<RX_BASE>::proto_fields, typename SympleFields<TX_BASE>::proto_fields>{};

    template<uint8_t *RX_BASE, uint8_t *TX_BASE>
    class ComplexProtocol:  public ::proto::ProtocolEndpoint<typename ComplexFields<RX_BASE>::proto_fields, typename ComplexFields<TX_BASE>::proto_fields>{};
}
