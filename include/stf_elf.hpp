#pragma once

#include <algorithm>
#include <cstdint>
#include <map>

#include "stf_address_range.hpp"
#include "stf_bin.hpp"

#include "elfio/elfio.hpp"

class STFElf : public STFBinary {
    private:
        ELFIO::elfio reader_;
        std::map<STFAddressRange, const ELFIO::segment*> segments_;
        uint64_t min_address_ = std::numeric_limits<uint64_t>::max();

        auto findSection_(const uint64_t address, const size_t num_bytes = 0) const {
            validateAddress_(address);

            const uint64_t end_address = address + num_bytes;
            const STFAddressRange key(address, end_address);
            auto it = segments_.lower_bound(key);
            if(it == segments_.end()) {
                --it;
            }

            if(it->first.startAddress() > address) {
                --it;
            }

            if(address < it->first.startAddress() || address >= it->first.endAddress()) {
                return segments_.end();
            }

            return it;
        }

        void read_(const uint64_t address, void* const ptr, const size_t num_bytes) const final {
            auto it = findSection_(address, num_bytes);

            stf_assert(it != segments_.end(),
                       "Failed to find address " << std::hex << address);

            memcpy(reinterpret_cast<uint8_t*>(ptr),
                   it->second->get_data() + address - it->first.startAddress(),
                   num_bytes);
        }

        void open_() final {
            stf_assert(reader_.load(filename_), "Failed to load ELF " << filename_);
            for(unsigned int i = 0; i < reader_.segments.size(); ++i) {
                const auto* segment = reader_.segments[i];
                if(segment->get_type() == ELFIO::PT_LOAD) {
                    const auto start_addr = segment->get_virtual_address();
                    const auto end_addr = start_addr + segment->get_memory_size();
                    if(start_addr != end_addr) {
                        segments_.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(start_addr, end_addr),
                                          std::forward_as_tuple(segment));
                        file_size_ = std::max(file_size_, static_cast<size_t>(end_addr));
                        min_address_ = std::min(min_address_, start_addr);
                    }
                }
            }
        }

    public:
        STFElf() :
            STFBinary(0)
        {
        }

        explicit STFElf(const std::string& filename) :
            STFBinary(filename)
        {
            open_();
        }

        inline const auto& getReader() const {
            return reader_;
        }

        inline uint64_t getMinAddress() const {
            return min_address_;
        }

        inline uint64_t getMaxAddress() const {
            return file_size_;
        }
};

