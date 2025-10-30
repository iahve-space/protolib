#pragma once

template<typename RX_FIELDS, typename TX_FIELDS>
class FieldEndpoint {
public:
    virtual ~FieldEndpoint() = default;
    virtual void OnFieldUpdated() = 0;
};