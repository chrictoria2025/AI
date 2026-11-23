#pragma once

#include "IFlashStorage.hpp"

#include <cstdio>
#include <string>

namespace uav {

class FileFlashStorage : public IFlashStorage {
public:
    explicit FileFlashStorage(const std::string& filename, size_t capacityBytes = 256 * 96)
        : filename_(filename)
        , capacity_(capacityBytes)
    {
        file_ = std::fopen(filename_.c_str(), "w+b");
    }

    ~FileFlashStorage()
    {
        if (file_) {
            std::fclose(file_);
        }
    }

    FileFlashStorage(const FileFlashStorage&) = delete;
    FileFlashStorage& operator=(const FileFlashStorage&) = delete;

    bool write(const uint8_t* data, size_t offset, size_t length) override
    {
        if (!file_ || offset + length > capacity_) {
            return false;
        }

        if (std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) {
            return false;
        }

        size_t written = std::fwrite(data, 1, length, file_);
        std::fflush(file_);
        return written == length;
    }

    bool read(uint8_t* buffer, size_t offset, size_t length) override
    {
        if (!file_ || offset + length > capacity_) {
            return false;
        }

        if (std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) {
            return false;
        }

        size_t bytesRead = std::fread(buffer, 1, length, file_);
        return bytesRead == length;
    }

    size_t capacity() const override
    {
        return capacity_;
    }

private:
    std::string filename_;
    size_t capacity_;
    FILE* file_{nullptr};
};

} // namespace uav
