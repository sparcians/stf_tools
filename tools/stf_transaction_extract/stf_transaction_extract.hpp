#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "stf_enums.hpp"
#include "stf_transaction_reader.hpp"
#include "stf_record_types.hpp"
#include "stf_transaction_writer.hpp"

/**
 * \struct STFTransactionExtractConfig
 * Holds stf_transaction_extract configuration parsed from command line
 */
struct STFTransactionExtractConfig {
    std::string trace_filename; /**< Trace filename to open */
    uint64_t head_count = 0; /**< Number of transactions to extract from beginning of trace */
    uint64_t skip_count = 0; /**< Number of transactions to skip */
    uint64_t split_count = 0; /**< Splits trace into segments split_count transactions long */
    std::string output_filename = "-"; /**< By default, go to stdout. */
};

/**
 * \class STFTransactionExtractor
 * Does the actual work of extracting from an STF
 */
class STFTransactionExtractor {
    private:
        stf::STFTransactionReader stf_reader_; /**< STF reader to use */
        stf::STFTransactionReader::iterator transaction_it_;
        stf::STFTransactionWriter stf_writer_; /**< STF writer to use */
        stf::RecordMap record_map_;

        std::vector<std::string> comments_; /**< Tracks comment records */

        void processTransaction_(const stf::STFTransaction& transaction,
                                 uint64_t& transaction_count,
                                 const bool extracting = false) {
            ++transaction_count;

            for(const auto& c: transaction.getComments()) {
                comments_.emplace_back(c->as<stf::CommentRecord>().getData());
            }
        }

        /**
         * \brief Skip the first skipcount transactions and build up the TLB page table;
         * return number of transaction skipped
         */
        uint64_t extractSkip_(const uint64_t skipcount) {
            uint64_t skipped = 0;

            // Process trace records
            while ((transaction_it_ != stf_reader_.end()) && (skipped < skipcount)) {
                processTransaction_(*transaction_it_, skipped);
                ++transaction_it_;
            }

            return skipped;
        }

        /**
         * \brief extract max_transaction_count transactions; and update TLB page table;
         */
        uint64_t extractTxn_(const uint64_t max_transaction_count, const bool modify_header) {
            uint64_t count = 0;

            // Indicate the input file had been read in the past;
            // The trace_info and transaction initial info, such as
            // pc, physical pc are required to written in new output file.
            stf_reader_.copyHeader(stf_writer_);
            stf_writer_.addTraceInfo(stf::STF_GEN::STF_GEN_STF_TRANSACTION_EXTRACT,
                                     TRACE_TOOLS_VERSION_MAJOR,
                                     TRACE_TOOLS_VERSION_MINOR,
                                     TRACE_TOOLS_VERSION_MINOR_MINOR,
                                     "Trace extracted with stf_transaction_extract");

            if (modify_header) {
                stf_writer_.addHeaderComments(comments_);
                stf_writer_.finalizeHeader();
            }
            else {
                stf_writer_.finalizeHeader();
            }

            std::vector<stf::STFRecord::UniqueHandle> transaction_records;

            // Process trace records
            while ((transaction_it_ != stf_reader_.end()) && (count < max_transaction_count)) {
                processTransaction_(*transaction_it_, count, true);
                transaction_it_->write(stf_writer_);
                ++transaction_it_;
            }

            std::cerr << "Output " << count << " transactions" << std::endl;
            return count;
        }


    public:
        /**
         * Runs the extractor
         * \param head_count Number of transactions to extract from the beginning of the trace
         * \param skip_count Number of transactions to skip
         * \param split_count If > 0, split trace into split_count length segments
         * \param output_filename Output filename to write
         */
        void run(uint64_t head_count, const uint64_t skip_count, const uint64_t split_count, const std::string& output_filename) {
            uint64_t overall_transaction_count = extractSkip_(skip_count);

            stf_assert(overall_transaction_count == skip_count,
                       "Specified skip count (" << skip_count << ") was greater than the trace length (" << overall_transaction_count << ").");

            if (split_count > 0) {
                uint32_t file_count = 0;
                bool modify_header = overall_transaction_count;

                while(true) {
                    uint64_t transaction_written = 0;

                    const std::string cur_output_file = output_filename + '.' + std::to_string(file_count++) + ".zstf";
                    // open new stf file for slicing;
                    std::cerr << overall_transaction_count << " Creating split trace file " << cur_output_file << std::endl;
                    stf_writer_.open(cur_output_file);
                    stf_assert(stf_writer_, "Error: Failed to open output " << cur_output_file);

                    transaction_written = extractTxn_(split_count, modify_header);
                    modify_header = true;
                    stf_writer_.close();

                    if (transaction_written < split_count) {
                        break;
                    }
                }
            }
            else {
                if (head_count == 0) {
                    head_count = std::numeric_limits<uint64_t>::max();
                }

                stf_writer_.open(output_filename);
                stf_assert(stf_writer_, "Error: Failed to open output " << output_filename);
                extractTxn_(head_count, overall_transaction_count);
                stf_writer_.close();
            }
        }

        /**
         * Constructs an STFTransactionExtractor
         * \param config Config to use
         */
        explicit STFTransactionExtractor(const STFTransactionExtractConfig& config) :
            stf_reader_(config.trace_filename),
            transaction_it_(stf_reader_.begin())
        {
        }
};
