#pragma once

#include <cinttypes>
#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <boost/container_hash/hash.hpp>

#include "print_utils.hpp"
#include "stf_enums.hpp"
#include "stf_exception.hpp"
#include "stf_inst.hpp"

static constexpr size_t MAX_COUNT_LENGTH = 20; /**< Maximum length of a uint64_t in decimal format */

/**
 * \enum ErrorCode
 * Lists all error types detected by stf_check
 */
enum class ErrorCode : uint8_t {
    RESERVED_NO_ERROR   = 0,    // no error
    INVALID_PC_32       = 1,    // invalid pc value found (32 bit opcode)
    INVALID_PC_16       = 2,    // invalid pc value found (16 bit opcode)
    HEADER              = 3,    // unknown trace generator or missing trace header
    INVALID_INST        = 4,    // invalid instruction
    EMBED_PTE           = 5,    // stf contains/doesn't contain PTEs when it should/shouldn't
    PA_EQ_0             = 6,    // physical address == 0
    PA_NE_VA            = 7,    // 0xfff of paddr != 0xfff of vaddr
    INVALID_PHYS        = 8,    // invalid phys pc value
    PHYS_ADDR           = 9,    // stf contains/doesn't contain physical addresses when it should/shouldn't
    PTE_HEADER          = 10,   // PTE values found in header
    RV64_INSTS          = 11,   // stf contains/doesn't contain RV64 instructions when it should/shouldn't
    MEM_ATTR            = 12,   // instruction accesses memory but missing access attribute record
    PREFETCH            = 13,   // prefetch instruction does not have data virtual address
    MISS_MEM            = 14,   // missing memory access record
    MISS_MEM_LOAD       = 15,   // missing memory access record for load
    MISS_MEM_STR        = 16,   // missing memory access record for store
    UNCOND_BR           = 17,   // unconditional branch instr does not have PC target
    SWITCH_USR          = 18,   // switch to user mode wihtout sret/mret instr
    MEM_POINT_TO_ZERO   = 19,   // store write to vaddr zero
    DECODER_FAILURE     = 20,   // decoder failed to recognize an instruction
    PC_DISCONTINUITY    = 21,   // pc jumps to the wrong address
    RESERVED_NUM_ERRORS
};

/**
 * Formats an ErrorCode to an ostream
 * \param os std::ostream to send output to
 * \param code ErrorCode to format
 */
inline std::ostream& operator<<(std::ostream& os, const ErrorCode code) {
    switch(code) {
        case ErrorCode::RESERVED_NO_ERROR:
            os << "NO_ERROR";
            return os;
        case ErrorCode::INVALID_PC_32:
            os << "INVALID_PC_32";
            return os;
        case ErrorCode::INVALID_PC_16:
            os << "INVALID_PC_16";
            return os;
        case ErrorCode::HEADER:
            os << "HEADER";
            return os;
        case ErrorCode::INVALID_INST:
            os << "INVALID_INST";
            return os;
        case ErrorCode::EMBED_PTE:
            os << "EMBED_PTE";
            return os;
        case ErrorCode::PA_EQ_0:
            os << "PA_EQ_0";
            return os;
        case ErrorCode::PA_NE_VA:
            os << "PA_NE_VA";
            return os;
        case ErrorCode::INVALID_PHYS:
            os << "INVALID_PHYS";
            return os;
        case ErrorCode::PHYS_ADDR:
            os << "PHYS_ADDR";
            return os;
        case ErrorCode::PTE_HEADER:
            os << "PTE_HEADER";
            return os;
        case ErrorCode::RV64_INSTS:
            os << "RV64_INSTS";
            return os;
        case ErrorCode::MEM_ATTR:
            os << "MEM_ATTR";
            return os;
        case ErrorCode::PREFETCH:
            os << "PREFETCH";
            return os;
        case ErrorCode::MISS_MEM:
            os << "MISS_MEM";
            return os;
        case ErrorCode::MISS_MEM_LOAD:
            os << "MISS_MEM_LOAD";
            return os;
        case ErrorCode::MISS_MEM_STR:
            os << "MISS_MEM_STR";
            return os;
        case ErrorCode::UNCOND_BR:
            os << "UNCOND_BR";
            return os;
        case ErrorCode::SWITCH_USR:
            os << "SWITCH_USR";
            return os;
        case ErrorCode::MEM_POINT_TO_ZERO:
            os << "MEM_POINT_TO_ZERO";
            return os;
        case ErrorCode::DECODER_FAILURE:
            os << "DECODER_FAILURE";
            return os;
        case ErrorCode::PC_DISCONTINUITY:
            os << "PC_DISCONTINUITY";
            return os;
        case ErrorCode::RESERVED_NUM_ERRORS:
            break;
    };

    stf_throw("Invalid ErrorCode: " << stf::enums::to_printable_int(code));

    return os;
}

#define PARSER_ENTRY(x) { #x, ErrorCode::x }

inline ErrorCode parseErrorCode(const std::string_view err_code_str) {
    static const std::unordered_map<std::string_view, ErrorCode> string_map {
        PARSER_ENTRY(INVALID_PC_32),
        PARSER_ENTRY(INVALID_PC_16),
        PARSER_ENTRY(HEADER),
        PARSER_ENTRY(INVALID_INST),
        PARSER_ENTRY(EMBED_PTE),
        PARSER_ENTRY(PA_EQ_0),
        PARSER_ENTRY(PA_NE_VA),
        PARSER_ENTRY(INVALID_PHYS),
        PARSER_ENTRY(PHYS_ADDR),
        PARSER_ENTRY(PTE_HEADER),
        PARSER_ENTRY(RV64_INSTS),
        PARSER_ENTRY(MEM_ATTR),
        PARSER_ENTRY(PREFETCH),
        PARSER_ENTRY(MISS_MEM),
        PARSER_ENTRY(MISS_MEM_LOAD),
        PARSER_ENTRY(MISS_MEM_STR),
        PARSER_ENTRY(UNCOND_BR),
        PARSER_ENTRY(SWITCH_USR),
        PARSER_ENTRY(MEM_POINT_TO_ZERO),
        PARSER_ENTRY(DECODER_FAILURE),
        PARSER_ENTRY(PC_DISCONTINUITY)
    };

    const auto it = string_map.find(err_code_str);
    stf_assert(it != string_map.end(), "Invalid error code specified: " << err_code_str);
    return it->second;
}

#undef PARSER_ENTRY

/**
 * \class ErrorTracker
 * Counts and classifies errors found in an STF
 */
class ErrorTracker {
    private:
        static constexpr int COUNT_COLUMN_WIDTH_ = MAX_COUNT_LENGTH + 4; /**< Width of the error count column */
        static_assert(COUNT_COLUMN_WIDTH_ >= MAX_COUNT_LENGTH, "COUNT_COLUMN_WIDTH_ must be >= MAX_COUNT_LENGTH");
        static constexpr int ERROR_CODE_COLUMN_WIDTH_ = 25; /*< Width of the error code column */
        static constexpr int DESCRIPTION_COLUMN_WIDTH_ = 0; /*< Width of the error description column. Currently the last column so width doesn't matter, but making this configurable just in case */

        /**
         * \class ErrorOstream
         * Subclass of std::ostream that provides a mechanism to immediately exit the program after printing an error.
         * This is configured with the setContinueOnError method - if we call setContinueOnError(false) then it will exit after
         * printing the first error. Otherwise, it will act like a normal std::ostream.
         */
        class ErrorOstream : public std::ostream {
            private:
                /**
                 * \class ErrorOstreamBuf
                 * Subclass of std::stringbuf that optionally exits the program when its parent ostream is
                 * flushed - i.e. when an error is printed.
                 */
                class ErrorOstreamBuf final : public std::stringbuf {
                    private:
                        std::ostream& os_; /**< Underlying ostream that gets our output */
                        bool continue_on_error_ = false; /**< If false, will exit the program after printing the first error */
                        bool multiple_errors_ = false; /**< If true, we have detected multiple errors */
                        ErrorCode return_code_ = ErrorCode::RESERVED_NO_ERROR; /**< Used as the exit code when exiting due to an error */
                        bool ignore_next_error_ = false;

                        /**
                         * Gets the exit code as an int so we can pass it to exit()
                         */
                        int getExitCode_() const {
                            static constexpr int MULTIPLE_ERRORS = 255;
                            if(multiple_errors_) {
                                return MULTIPLE_ERRORS;
                            }

                            return static_cast<int>(return_code_);
                        }

                        /**
                         * Actually write output to the stream, optionally exiting if there was an error
                         */
                        void putOutput_() {
                            // Called by destructor.
                            // destructor can not call virtual methods.
                            if(STF_EXPECT_TRUE(!ignore_next_error_)) {
                                os_ << str();
                            }
                            str("");
                            os_.flush();
                            if(STF_EXPECT_FALSE(ignore_next_error_)) {
                                ignore_next_error_ = false;
                            }
                            else if(!continue_on_error_) {
                                exit(getExitCode_()); // Exit immediately with specific error code.
                            }
                        }

                    public:
                        /**
                         * Constructs an ErrorOstreamBuf
                         * \param os Underlying std::ostream
                         * \param continue_on_error if false, exit after the first error
                         */
                        explicit ErrorOstreamBuf(std::ostream& os, bool continue_on_error = false) :
                            os_(os),
                            continue_on_error_(continue_on_error)
                        {
                        }

                        ~ErrorOstreamBuf() final {
                            if(pbase() != pptr()) {
                                putOutput_();
                            }
                        }

                        /**
                         * Set whether the program should exit on the first error
                         * \param continue_on_error If false, exit on the first error
                         */
                        void setContinueOnError(bool continue_on_error) {
                            continue_on_error_ = continue_on_error;
                        }

                        /**
                         * Get whether the program should exit on the first error
                         */
                        bool continueOnError() const {
                            return continue_on_error_;
                        }

                        /**
                         * Set the exit code (only used when there is no error)
                         * \param return_code Exit code to set
                         */
                        void setReturnCode(ErrorCode return_code) {
                            return_code_ = return_code;
                        }

                        /**
                         * Get the exit code
                         */
                        ErrorCode getReturnCode() const {
                            return return_code_;
                        }

                        /**
                         * Set the exit code when there is an error
                         * \param error_code Exit code to set
                         */
                        void setErrorCode(ErrorCode error_code) {
                            if (return_code_ == ErrorCode::RESERVED_NO_ERROR) { // This is the first reported error.
                                return_code_ = error_code;
                            }
                            else if (return_code_ != error_code) {
                                multiple_errors_ = true; // Indicate multiple errors.
                            }
                        }

                        /**
                         * Syncs the stream with the output
                         */
                        int sync() final {
                            putOutput_();
                            return 0;
                        }

                        void ignoreNextError() {
                            ignore_next_error_ = true;
                        }
                };

                ErrorOstreamBuf os_; /**< Underlying streambuf used to process output */

            public:
                /**
                 * Constructs an ErrorOstream
                 * \param os Underlying std::ostream
                 * \param continue_on_error If false, the program will exit on the first error
                 */
                explicit ErrorOstream(std::ostream& os, bool continue_on_error = false) :
                    std::ostream(&os_),
                    os_(os, continue_on_error)
                {
                }

                /**
                 * Set whether the program should exit on the first error
                 * \param continue_on_error If false, exit on the first error
                 */
                void setContinueOnError(bool continue_on_error) {
                    os_.setContinueOnError(continue_on_error);
                }

                /**
                 * Get whether the program should exit on the first error
                 */
                bool continueOnError() const {
                    return os_.continueOnError();
                }

                /**
                 * Set the exit code (only used when there is no error)
                 * \param return_code Exit code to set
                 */
                void setReturnCode(ErrorCode return_code) {
                    os_.setReturnCode(return_code);
                }

                /**
                 * Get the exit code
                 */
                ErrorCode getReturnCode() const {
                    return os_.getReturnCode();
                }

                /**
                 * Set the exit code when there is an error
                 * \param error_code Exit code to set
                 */
                void setErrorCode(ErrorCode error_code) {
                    os_.setErrorCode(error_code);
                }

                void ignoreNextError() {
                    os_.ignoreNextError();
                }
        };

        static const std::map<ErrorCode, const char*> error_code_msgs_; /**< Maps error codes to specific error messages */

        std::array<uint64_t, stf::enums::to_int(ErrorCode::RESERVED_NUM_ERRORS) - 1> errors_ = {0}; /**< Holds error counts */
        const std::unordered_set<ErrorCode> ignored_errors_; /**< Holds error codes that should be ignored */

        ErrorOstream err_; /**< Used to report error messages */

        /**
         * Prints the number of occurrences of the specified error type with an accompanying message
         * \param code Error type
         * \param msg Message to include with the count
         */
        void printErrorCount_(const ErrorCode code, const std::string_view msg) const {
            stf::print_utils::printDec(getErrorCount(code), MAX_COUNT_LENGTH);
            stf::print_utils::printSpaces(COUNT_COLUMN_WIDTH_ - MAX_COUNT_LENGTH);
            stf::print_utils::printLeft(code, ERROR_CODE_COLUMN_WIDTH_);
            stf::print_utils::printLeft(msg, DESCRIPTION_COLUMN_WIDTH_);
            std::cout << std::endl;
        }

        /**
         * Checks whether the specified error code is a valid ErrorCode enum value
         * \param code Code to check
         */
        static inline void validateCode_(ErrorCode code) {
            static constexpr auto RESERVED_NO_ERROR = stf::enums::to_int(ErrorCode::RESERVED_NO_ERROR);
            static constexpr auto RESERVED_NUM_ERRORS = stf::enums::to_int(ErrorCode::RESERVED_NUM_ERRORS);
            const auto code_int = stf::enums::to_int(code);
            stf_assert(code_int > RESERVED_NO_ERROR && code_int < RESERVED_NUM_ERRORS, "Invalid error code: " << stf::enums::to_printable_int(code));
        }

    public:
        explicit ErrorTracker(const std::unordered_set<ErrorCode>& ignored_errors) :
            ignored_errors_(ignored_errors),
            err_(std::cerr)
        {
        }

        /**
         * Get whether the program should exit on the first error
         */
        bool continueOnError() const {
            return err_.continueOnError();
        }

        /**
         * Set whether the program should exit on the first error
         */
        void setContinueOnError(bool continue_on_error) {
            err_.setContinueOnError(continue_on_error);
        }

        /**
         * Increment the count for the specified error type
         * \param code Error type
         */
        void countError(const ErrorCode code) {
            validateCode_(code);
            ++errors_[stf::enums::to_int(code) - 1];
        }

        /**
         * Get the count for the specified error type
         * \param code Error type
         */
        uint64_t getErrorCount(const ErrorCode code) const {
            if(code == ErrorCode::RESERVED_NO_ERROR) {
                return 0;
            }
            validateCode_(code);
            return errors_[stf::enums::to_int(code) - 1];
        }

        /**
         * Report the specified error
         * \param code Error type
         */
        ErrorOstream& reportError(const ErrorCode code) {
            if(STF_EXPECT_TRUE(!ignored_errors_.count(code))) {
                err_.setErrorCode(code);
                err_ << "stf_check: ERROR " << code << ": ";
            }
            else {
                err_.ignoreNextError();
            }

            return err_;
        }

        /**
         * Get the current exit code
         */
        ErrorCode getReturnCode() const {
            return err_.getReturnCode();
        }

        /**
         * Set the exit code (only used when there is no error)
         * \param return_code Exit code to set
         */
        void setReturnCode(ErrorCode return_code) {
            err_.setReturnCode(return_code);
        }

        /**
         * Print the counts for every error type
         */
        void printErrorCounts() const {
            stf::print_utils::printLeft("Error Count", COUNT_COLUMN_WIDTH_);
            stf::print_utils::printLeft("Error Code", ERROR_CODE_COLUMN_WIDTH_);
            stf::print_utils::printLeft("Description", DESCRIPTION_COLUMN_WIDTH_);
            std::cout << std::endl;
            for(const auto& p: error_code_msgs_) {
                printErrorCount_(p.first, p.second);
            }
        }
};

inline const std::map<ErrorCode, const char*> ErrorTracker::error_code_msgs_ = {
    {ErrorCode::RESERVED_NO_ERROR, "no error"},
    {ErrorCode::INVALID_PC_32, "invalid pc value found in mode"},
    {ErrorCode::INVALID_PC_16, "invalid pc value found in mode"},
    {ErrorCode::HEADER, "unknown trace generator or missing trace header"},
    {ErrorCode::INVALID_INST, "invalid instruction"},
    {ErrorCode::EMBED_PTE, "stf contains/doesn't contain PTEs when it should/shouldn't"},
    {ErrorCode::PA_EQ_0, "physical address == 0"},
    {ErrorCode::PA_NE_VA, "0xfff of paddr != 0xfff of vaddr"},
    {ErrorCode::INVALID_PHYS, "invalid phys pc value"},
    {ErrorCode::PHYS_ADDR, "stf contains/doesn't contain physical addresses when it should/shouldn't"},
    {ErrorCode::PTE_HEADER, "PTE values found in header"},
    {ErrorCode::RV64_INSTS, "stf contains/doesn't contain RV64 instructions when it should/shouldn't"},
    {ErrorCode::MEM_ATTR, "instruction accesses memory but missing access attribute record"},
    {ErrorCode::PREFETCH, "prefetch instruction does not have data virtual address"},
    {ErrorCode::MISS_MEM, "missing memory access record"},
    {ErrorCode::MISS_MEM_LOAD, "missing memory access record for load"},
    {ErrorCode::MISS_MEM_STR, "missing memory access record for store"},
    {ErrorCode::UNCOND_BR, "unconditional branch instr does not have PC target"},
    {ErrorCode::SWITCH_USR, "switch to user mode wihtout sret/mret instr"},
    {ErrorCode::MEM_POINT_TO_ZERO, "Memory records point to virtual address zero"},
    {ErrorCode::PC_DISCONTINUITY, "PC jumps to the wrong address"}
};

/**
 * \struct STFCheckConfig
 * Holds the configuration for stf_check parsed from command line arguments
 */
struct STFCheckConfig {
    std::string trace_filename; /**< Filename of trace to check */
    bool skip_non_user = false; /**< If true, skip non-user mode instructions when checking the trace */
    bool print_memory_zero_warnings = false; /**< Whether we should print warnings if a memory record accesses address 0x0 */
    bool print_info = false; /**< Whether we should print trace info */
    bool check_phys_addr = true; /**< Whether we should check physical addresses */
    bool continue_on_error = false; /**< Whether we should continue on an error */
    bool always_print_error_counts = false; /**< Whether we should always print error counts at the end */
    uint64_t end_inst = 0; /**< If > 0, stop checking after this many instructions */
    std::unordered_set<ErrorCode> ignored_errors; /*< Contains error codes that should be ignored */
};

/**
 * \typedef ThreadMapKey
 * Key type used for ThreadMap
 * Combines TID, TGID, and ASID
 */
using ThreadMapKey = std::tuple<uint32_t, uint32_t, uint32_t>;

/**
 * \typedef ThreadMap
 * Maps TID, TGID, and ASID to an STFInst
 */
using ThreadMap = std::unordered_map<ThreadMapKey, stf::STFInst, boost::hash<ThreadMapKey>>;
