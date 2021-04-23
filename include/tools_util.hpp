#pragma once

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "filesystem.hpp"
#include "stf.hpp"
#include "trace_tools_git_version.hpp"

/// Current STF tool version
static constexpr uint32_t TRACE_TOOLS_VERSION_MAJOR = 1;
static constexpr uint32_t TRACE_TOOLS_VERSION_MINOR = 0;
static constexpr uint32_t TRACE_TOOLS_VERSION_MINOR_MINOR = 0;

template<typename T, int radix = 10>
static inline T parseInt(const std::string_view str) {
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
static inline T parseHex(const std::string& str) {
    static constexpr int HEX_RADIX = 16;
    return parseInt<T, HEX_RADIX>(str);
}

static inline void getVersion(std::ostream& os) {
    os << "trace_tools version "
       << TRACE_TOOLS_VERSION_MAJOR << '.'
       << TRACE_TOOLS_VERSION_MINOR << '.'
       << TRACE_TOOLS_VERSION_MINOR_MINOR;
    os << ", trace_tools commit SHA " << TRACE_TOOLS_GIT_VERSION << std::endl;
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

static inline uint64_t log2(const uint64_t x) {
    stf_assert(x != 0, "Attempted to take log2(0)");
    static constexpr uint64_t NUM_U64_BITS = 8 * sizeof(uint64_t);
    return NUM_U64_BITS - static_cast<uint64_t>(__builtin_clzl(x)) - 1;
}

static inline fs::path getExecutablePath() {
#ifdef __APPLE__ // OSX
    static constexpr uint32_t DEFAULT_BUF_SIZE = 256;
    uint32_t buf_size = DEFAULT_BUF_SIZE;
    std::unique_ptr<char[]> path_buf = std::make_unique<char[]>(DEFAULT_BUF_SIZE);

    if(_NSGetExecutablePath(path_buf.get(), &buf_size) != 0) {
        // path_buf was too small, so reallocate and try again
        path_buf = std::make_unique<char[]>(buf_size);
        stf_assert(_NSGetExecutablePath(path_buf.get(), &buf_size) == 0, "Failed to get path to exe");
    }

    const char* const relative_path = path_buf.get();

    // fs::read_symlink will throw an exception if it isn't a symlink, so check first
    if(!fs::is_symlink(relative_path)) {
        return fs::canonical(relative_path);
    }
#else // Linux
    static const char* const relative_path = "/proc/self/exe"; // always a symlink
#endif
    return fs::canonical(fs::read_symlink(relative_path));
}
