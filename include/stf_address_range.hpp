#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "stf_exception.hpp"

class STFAddressRange {
    private:
        uint64_t start_pc_ = 0;
        uint64_t end_pc_ = 0;

        STFAddressRange() = default;

    public:
        static inline const STFAddressRange& invalid() {
            static const STFAddressRange invalid;
            return invalid;
        }

        explicit STFAddressRange(const uint64_t pc) :
            STFAddressRange(pc, pc + 1)
        {
        }

        STFAddressRange(const uint64_t start_pc, const uint64_t end_pc) :
            start_pc_(start_pc),
            end_pc_(end_pc)
        {
            stf_assert(start_pc_ < end_pc_, "End PC must be larger than start PC");
        }

        inline uint64_t startAddress() const {
            return start_pc_;
        }

        inline uint64_t endAddress() const {
            return end_pc_;
        }

        inline uint64_t range() const {
            return end_pc_ - start_pc_;
        }

        inline bool contains(const uint64_t pc) const {
            return contains(STFAddressRange(pc));
        }

        inline bool contains(const STFAddressRange& rhs) const {
            return start_pc_ <= rhs.start_pc_ && rhs.end_pc_ <= end_pc_;
        }

        inline bool startsBefore(const STFAddressRange& rhs) const {
            return start_pc_ < rhs.start_pc_;
        }

        inline bool startsAfter(const STFAddressRange& rhs) const {
            return start_pc_ > rhs.start_pc_;
        }

        inline bool operator<(const STFAddressRange& rhs) const {
            //return startsBefore(rhs) || ((start_pc_ == rhs.start_pc_) && (range() < rhs.range()));
            return rhs.contains(*this) || (!contains(rhs) && startsBefore(rhs));
        }

        inline bool operator>(const STFAddressRange& rhs) const {
            //return startsAfter(rhs) || ((start_pc_ == rhs.start_pc_) && (range() > rhs.range()));
            return contains(rhs) || (!rhs.contains(*this) && startsAfter(rhs));
        }

        inline bool operator==(const STFAddressRange& rhs) const {
            return (start_pc_ == rhs.start_pc_) && (end_pc_ == rhs.end_pc_);
        }

        inline bool operator<=(const STFAddressRange& rhs) const {
            return (*this < rhs) || (*this == rhs);
        }

        inline bool operator>=(const STFAddressRange& rhs) const {
            return (*this > rhs) || (*this == rhs);
        }
};
