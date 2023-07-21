#pragma once

#include <algorithm>
#include <cstdint>
#include <map>

#include "filesystem.hpp"
#include "stf_address_range.hpp"
#include "stf_bin.hpp"

#include "elfio/elfio.hpp"

class STFElf : public STFBinary {
    private:
        ELFIO::elfio reader_;
        std::map<STFAddressRange, const ELFIO::segment*> segments_;
        fs::path filename_;

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

        void read_(const uint64_t address, void* const ptr, const size_t num_bytes) const override {
            auto it = findSection_(address, num_bytes);

            stf_assert(it != segments_.end(),
                       "Failed to find address " << std::hex << address);

            memcpy(reinterpret_cast<uint8_t*>(ptr),
                   it->second->get_data() + address - it->first.startAddress(),
                   num_bytes);
        }

    public:
        STFElf() :
            STFBinary(0)
        {
        }

        explicit STFElf(const std::string_view filename) :
            STFElf()
        {
            open(filename);
        }

        void open(const std::string_view filename) final {
            filename_ = filename;

            stf_assert(reader_.load(filename.data()), "Failed to load ELF " << filename);
            for(unsigned int i = 0; i < reader_.segments.size(); ++i) {
                const auto* segment = reader_.segments[i];
                if(segment->get_type() == ELFIO::PT_LOAD) {
                    const auto start_addr = segment->get_virtual_address();
                    const auto end_addr = start_addr + segment->get_memory_size();
                    segments_.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(start_addr, end_addr),
                                      std::forward_as_tuple(segment));
                    file_size_ = std::max(file_size_, static_cast<size_t>(end_addr));
                }
            }
        }

        inline const auto& getFilename() const {
            return filename_;
        }

        inline const auto& getReader() const {
            return reader_;
        }
};

