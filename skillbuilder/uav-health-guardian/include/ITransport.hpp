#pragma once

#include <cstddef>
#include <cstdint>

namespace uav {

class ITransport {
public:
    virtual ~ITransport() = default;
    virtual void send(const uint8_t* data, size_t len) = 0;
};

} // namespace uav
