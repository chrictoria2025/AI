#pragma once

#include <cstddef>
#include <cstdint>

namespace uav {

class IFlashStorage {
public:
    virtual ~IFlashStorage() = default;
    virtual bool write(const uint8_t* data, size_t offset, size_t length) = 0;
    virtual bool read(uint8_t* buffer, size_t offset, size_t length) = 0;
    virtual size_t capacity() const = 0;
};

} // namespace uav
