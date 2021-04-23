#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "stf_exception.hpp"

/**
 * \class STFBinary
 *
 * Class that allows access to arbitrary binary data
 */
class STFBinary {
    protected:
        uint64_t base_addr_ = 0;
        void* file_ptr_ = nullptr;
        size_t file_size_ = 0;
        int fd_ = -1;

        void validateAddress_(const uint64_t address) const {
            stf_assert(address >= base_addr_ && address <= base_addr_ + file_size_, std::hex << "Address " <<
                                                                                    address <<
                                                                                    " is outside binary range: [" <<
                                                                                    base_addr_ <<
                                                                                    ',' <<
                                                                                    (base_addr_ + file_size_) <<
                                                                                    ']');
        }

        virtual void read_(const uint64_t address, void* const ptr, const size_t num_bytes) const {
            validateAddress_(address);
            memcpy(reinterpret_cast<uint8_t*>(ptr),
                   reinterpret_cast<uint8_t*>(file_ptr_) + (address - base_addr_),
                   num_bytes);
        }

    public:
        explicit STFBinary(const uint64_t base_addr) :
            base_addr_(base_addr)
        {
        }

        virtual ~STFBinary() {
            STFBinary::close();
        }

        virtual void close() {
            if(file_ptr_ && file_ptr_ != MAP_FAILED) {
                munmap(file_ptr_, file_size_);
            }
            if(fd_ >= 0) {
                ::close(fd_);
            }
        }

        virtual void open(const std::string_view filename) {
            fd_ = ::open(filename.data(), O_RDONLY);
            stf_assert(fd_ >= 0, "Failed to open " << filename << ": " << strerror(errno));
            file_size_ = static_cast<size_t>(lseek(fd_, 0, SEEK_END));
            file_ptr_ = mmap(nullptr, file_size_, PROT_READ, MAP_FILE | MAP_PRIVATE, fd_, 0);
            stf_assert(file_ptr_ && file_ptr_ != MAP_FAILED, "Failed to mmap file: " << strerror(errno));
        }

        template<typename T>
        T read(const uint64_t address) const {
            T value;
            read_(address, &value, sizeof(T));
            return value;
        }
};
