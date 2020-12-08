#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "stf_enums.hpp"
#include "stf_pc_tracker.hpp"
#include "stf_pte.hpp"
#include "stf_inst_reader.hpp"
#include "stf_record_types.hpp"
#include "stf_reg_state.hpp"
#include "stf_writer.hpp"

/**
 * \struct STFExtractConfig
 * Holds stf_extract configuration parsed from command line
 */
struct STFExtractConfig {
    std::string trace_filename; /**< Trace filename to open */
    uint64_t head_count = 0; /**< Number of instructions to extract from beginning of trace */
    uint64_t skip_count = 0; /**< Number of instructions to skip */
    uint64_t split_count = 0; /**< Splits trace into segments split_count instructions long */
    std::string output_filename = "-"; /**< By default, go to stdout. */
    uint64_t inst_offset = 0; /**< Offset instruction PCs by this address */
    bool filter_kernel_code = false; /**< If true, filter out kernel code */
    bool dump_ptes_on_demand = false; /**< If true, dump PTEs in line with instructions that need the translation */
};

/**
 * \class STFExtractor
 * Does the actual work of extracting from an STF
 */
class STFExtractor {
    private:
        stf::STFInstReader stf_reader_; /**< STF reader to use */
        stf::STFInstReader::iterator inst_it_;
        stf::STFWriter stf_writer_; /**< STF writer to use */
        stf::STF_PTE page_table_; /**< Tracks page table info */
        stf::STFRegState regstate_; /**< Tracks register state */
        stf::PCTracker pc_tracker_; /**< Tracks PC state */
        stf::RecordMap record_map_;

        const bool dump_ptes_on_demand_; /**< If true, dump PTEs in line with instructions that need the translation */

        std::vector<std::string> comments_; /**< Tracks comment records */

        /**
         * \brief Helper function to parse the Escape record for thread id information;
         *  return 1 if the record is opcode record, otherwise return 0;
         *
         * \param rec the STF record
         * \param page_table the live TLB page table without size limitation;
         */
        void processInst_(const stf::STFInst& inst,
                          uint64_t& inst_count,
                          const bool extracting = false) {
            ++inst_count;
            for(const auto& c: inst.getComments()) {
                comments_.emplace_back(c->as<stf::CommentRecord>().getData());
            }

            for(const auto& s: inst.getRegisterStates()) {
                regstate_.regStateUpdate(s.getRecord());
            }

            for(const auto& s: inst.getDestOperands()) {
                regstate_.regStateUpdate(s.getRecord());
            }

            for(const auto& p: inst.getEmbeddedPTEs()) {
                const auto result = record_map_.emplace(p->clone());
                const auto& pte_rec = result->as<stf::PageTableWalkRecord>();
                const_cast<stf::PageTableWalkRecord&>(pte_rec).setIndex(inst_count);
                page_table_.UpdatePTE(inst.asid(), &pte_rec);
            }

            pc_tracker_.track(inst);
        }

        /**
         * \brief Skip the first skipcount instructions and build up the TLB page table;
         * return number of instruction skipped
         */
        uint64_t extractSkip_(const uint64_t skipcount) {
            uint64_t skipped = 0;

            // Process trace records
            while ((inst_it_ != stf_reader_.end()) && (skipped < skipcount)) {
                processInst_(*inst_it_, skipped);
                ++inst_it_;
            }

            return skipped;
        }

        /**
         * \brief extract instcount instructions; and update TLB page table;
         */
        uint64_t extractInstr_(const uint64_t max_instcount, const bool modify_header) {
            uint64_t count = 0;

            // Indicate the input file had been read in the past;
            // The trace_info and instruction initial info, such as
            // pc, physical pc are required to written in new output file.
            stf_reader_.copyHeader(stf_writer_);
            stf_writer_.addTraceInfo(stf::TraceInfoRecord(stf::STF_GEN::STF_GEN_STF_EXTRACT,
                                                          stf::STF_TOOL_VERSION,
                                                          0,
                                                          0,
                                                          "Trace extracted with stf_extract"));

            if (modify_header) {
                stf_writer_.addHeaderComments(comments_);
                stf_writer_.setHeaderPC(pc_tracker_.getNextPC());
                stf_writer_.finalizeHeader();

                if (stf_reader_.getTraceFeatures()->hasFeature(stf::TRACE_FEATURES::STF_CONTAIN_PROCESS_ID)) {
                    stf_writer_ << stf::ProcessIDExtRecord(inst_it_->tgid(), inst_it_->tid(), inst_it_->asid());
                }

                regstate_.writeRegState(stf_writer_);

                if (stf_reader_.getTraceFeatures()->hasFeature(stf::TRACE_FEATURES::STF_CONTAIN_PTE)) {
                    if(!dump_ptes_on_demand_) {
                        page_table_.DumpPTEtoSTF(stf_writer_);
                    }
                    else {
                        page_table_.ResetUsage();

                        //Output first PTE for first PC
                        page_table_.CheckAndDumpNewPTESingle(stf_writer_,
                                                             inst_it_->asid(),
                                                             pc_tracker_.getNextPC(),
                                                             count,
                                                             4);
                    }
                }
            }
            else {
                stf_writer_.finalizeHeader();
            }

            std::vector<stf::STFRecord::UniqueHandle> inst_records;

            // Process trace records
            while ((inst_it_ != stf_reader_.end()) && (count < max_instcount)) {
                processInst_(*inst_it_, count, true);
                if(dump_ptes_on_demand_) {
                    for(const auto& mem_access: inst_it_->getMemoryAccesses()) {
                        page_table_.CheckAndDumpNewPTESingle(stf_writer_,
                                                             inst_it_->asid(),
                                                             mem_access.getAddress(),
                                                             count,
                                                             static_cast<uint32_t>(mem_access.getSize()));
                    }

                    if(inst_it_->isCoF()) {
                        page_table_.CheckAndDumpNewPTESingle(stf_writer_,
                                                             inst_it_->asid(),
                                                             inst_it_->pc(),
                                                             count,
                                                             4);
                    }

                    if(inst_it_->isTakenBranch()) {
                        page_table_.CheckAndDumpNewPTESingle(stf_writer_,
                                                             inst_it_->asid(),
                                                             inst_it_->branchTarget(),
                                                             count,
                                                             4);
                    }
                }
                inst_it_->write(stf_writer_);
                ++inst_it_;
            }

            std::cerr << "Output " << count << " instructions" << std::endl;
            return count;
        }


    public:
        /**
         * Runs the extractor
         * \param head_count Number of instructions to extract from the beginning of the trace
         * \param skip_count Number of instructions to skip
         * \param split_count If > 0, split trace into split_count length segments
         * \param output_filename Output filename to write
         */
        void run(uint64_t head_count, const uint64_t skip_count, const uint64_t split_count, const std::string& output_filename) {
            uint64_t overall_instcnt = extractSkip_(skip_count);

            stf_assert(overall_instcnt == skip_count,
                       "Specified skip count (" << skip_count << ") was greater than the trace length (" << overall_instcnt <<").");

            if (split_count > 0) {
                uint32_t file_count = 0;
                bool modify_header = overall_instcnt;

                while(true) {
                    uint64_t inst_written = 0;

                    const std::string cur_output_file = output_filename + '.' + std::to_string(file_count++) + ".zstf";
                    // open new stf file for slicing;
                    std::cerr << overall_instcnt << " Creating split trace file " << cur_output_file << std::endl;
                    stf_writer_.open(cur_output_file);
                    stf_assert(stf_writer_, "Error: Failed to open output " << cur_output_file);

                    inst_written = extractInstr_(split_count, modify_header);
                    modify_header = true;
                    stf_writer_.close();

                    if (inst_written < split_count) {
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
                extractInstr_(head_count, overall_instcnt);
                stf_writer_.close();
            }
        }

        /**
         * Constructs an STFExtractor
         * \param config Config to use
         */
        explicit STFExtractor(const STFExtractConfig& config) :
            stf_reader_(config.trace_filename, config.filter_kernel_code),
            inst_it_(stf_reader_.begin()),
            page_table_(nullptr, nullptr, true),
            regstate_(stf_reader_.getISA(), stf_reader_.getInitialIEM()),
            pc_tracker_(stf_reader_.getInitialPC(), config.inst_offset),
            dump_ptes_on_demand_(config.dump_ptes_on_demand || stf_reader_.getTraceFeatures()->hasFeature(stf::TRACE_FEATURES::STF_CONTAIN_PTE))
        {
        }
};
