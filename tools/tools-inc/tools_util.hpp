#pragma once

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

#include "stf.hpp"
#include "trace_tools_git_version.hpp"

template<typename T, int radix = 10>
inline T parseInt(const std::string_view str) {
    static_assert(sizeof(T) <= sizeof(uint64_t), "parseInt only works on integers up to 64 bits");
    std::string temp;
    std::copy_if(str.begin(), str.end(), std::back_inserter(temp), [](char c){ return c != ','; });

    if(sizeof(T) <= 4) {
        if(std::is_signed<T>::value) {
            return static_cast<T>(std::stol(temp, nullptr, radix));
        }

        return static_cast<T>(std::stoul(temp, nullptr, radix));
    }

    if(std::is_signed<T>::value) {
        return static_cast<T>(std::stoll(temp, nullptr, radix));
    }

    return static_cast<T>(std::stoull(temp, nullptr, radix));
}

template<typename T>
inline T parseHex(const std::string& str) {
    static constexpr int HEX_RADIX = 16;
    return parseInt<T, HEX_RADIX>(str);
}

static inline void getVersion(std::ostream& os) {
    os << "trace_tools commit SHA " << TRACE_TOOLS_GIT_VERSION << std::endl;
    stf::formatVersion(os);
}

static inline std::string getVersion() {
    std::ostringstream ss;
    getVersion(ss);
    return ss.str();
}

static inline void printVersion() {
    getVersion(std::cout);
}

inline constexpr uint64_t log2_expr(const uint64_t x) {
    constexpr uint64_t NUM_U64_BITS = 8 * sizeof(uint64_t);
    return NUM_U64_BITS - static_cast<uint64_t>(__builtin_clzl(x)) - 1;
}

inline uint64_t log2(const uint64_t x) {
    stf_assert(x != 0, "Attempted to take log2(0)");
    static constexpr uint64_t NUM_U64_BITS = 8 * sizeof(uint64_t);
    return NUM_U64_BITS - static_cast<uint64_t>(__builtin_clzl(x)) - 1;
}
