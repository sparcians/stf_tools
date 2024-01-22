#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stf_pte.hpp"
#include "stf_reader.hpp"
#include "stf_writer.hpp"
#include "stf_record_map.hpp"
#include "util.hpp"

struct ExtractFileInfo {
    const uint64_t start;
    const uint64_t end;
    const uint64_t repeat;
    const std::string filename;

    ExtractFileInfo(const uint64_t start,
                    const uint64_t end,
                    const uint64_t repeat,
                    std::string filename) :
        start(start),
        end(end),
        repeat(repeat),
        filename(std::move(filename))
    {
    }
};

using FileList = std::vector<ExtractFileInfo>;

class STFMergeExtractor {
    private:
        uint32_t inst_hw_tid_ = 0;
        uint32_t inst_pid_ = 0;
        uint32_t inst_tid_ = 0;
        stf::RecordMap record_map_;

    public:
        /**
         * \brief Helper function to parse the Escape record for thread id information;
         *  return 1 if the record is opcode record, otherwise return 0;
         *
         * \param rec the STF record
         * \param page_table the live TLB page table without size limitation;
         */
        void processRecord(stf::STFRecord::UniqueHandle& rec,
                           stf::STF_PTE& page_table,
                           const uint64_t inst_count) {
            // keep track of initial values of PC and INST_IEM
            switch (rec->getId()) {
                case stf::descriptors::internal::Descriptor::STF_PROCESS_ID_EXT:
                    {
                        const auto& pid_rec = rec->as<stf::ProcessIDExtRecord>();
                        inst_hw_tid_ = pid_rec.getHardwareTID();
                        inst_pid_ = pid_rec.getPID();
                        inst_tid_ = pid_rec.getTID();
                    }
                    break;

                case stf::descriptors::internal::Descriptor::STF_PAGE_TABLE_WALK:
                    {
                        const auto result = record_map_.emplace(stf::STFRecord::grabOwnership<stf::PageTableWalkRecord>(rec));
                        const auto& pte_rec = result->as<stf::PageTableWalkRecord>();
                        const_cast<stf::PageTableWalkRecord&>(pte_rec).setIndex(inst_count + 1);
                        page_table.UpdatePTE(inst_pid_, &pte_rec);
                    }
                    break;

                case stf::descriptors::internal::Descriptor::STF_RESERVED:
                case stf::descriptors::internal::Descriptor::STF_IDENTIFIER:
                case stf::descriptors::internal::Descriptor::STF_VERSION:
                case stf::descriptors::internal::Descriptor::STF_ISA:
                case stf::descriptors::internal::Descriptor::STF_TRACE_INFO_FEATURE:
                case stf::descriptors::internal::Descriptor::STF_VLEN_CONFIG:
                case stf::descriptors::internal::Descriptor::STF_PROTOCOL_ID:
                case stf::descriptors::internal::Descriptor::STF_CLOCK_ID:
                case stf::descriptors::internal::Descriptor::STF_END_HEADER:
                case stf::descriptors::internal::Descriptor::STF_INST_REG:
                case stf::descriptors::internal::Descriptor::STF_INST_READY_REG:
                case stf::descriptors::internal::Descriptor::STF_INST_MEM_ACCESS:
                case stf::descriptors::internal::Descriptor::STF_INST_MEM_CONTENT:
                case stf::descriptors::internal::Descriptor::STF_BUS_MASTER_ACCESS:
                case stf::descriptors::internal::Descriptor::STF_BUS_MASTER_CONTENT:
                case stf::descriptors::internal::Descriptor::STF_EVENT:
                case stf::descriptors::internal::Descriptor::STF_EVENT_PC_TARGET:
                case stf::descriptors::internal::Descriptor::STF_INST_MICROOP:
                case stf::descriptors::internal::Descriptor::__RESERVED_END:
                case stf::descriptors::internal::Descriptor::STF_TRACE_INFO:
                case stf::descriptors::internal::Descriptor::STF_COMMENT:
                case stf::descriptors::internal::Descriptor::STF_FORCE_PC:
                case stf::descriptors::internal::Descriptor::STF_INST_PC_TARGET:
                case stf::descriptors::internal::Descriptor::STF_INST_OPCODE16:
                case stf::descriptors::internal::Descriptor::STF_INST_OPCODE32:
                case stf::descriptors::internal::Descriptor::STF_INST_IEM:
                case stf::descriptors::internal::Descriptor::STF_TRANSACTION:
                case stf::descriptors::internal::Descriptor::STF_TRANSACTION_DEPENDENCY:
                    break;
            }
        }

        /**
         * \brief Skip the first skipcount instructions and build up the TLB page table;
         * return number of instructions skipped
         */
        uint64_t extractSkip(stf::STFReader& stf_reader,
                              const uint64_t skipcount,
                              stf::STF_PTE &page_table) {
            stf::STFRecord::UniqueHandle rec;

            const uint64_t num_insts_read = stf_reader.numInstsRead();
            const uint64_t end_inst = num_insts_read + skipcount;
            // Process trace records
            try {
                while ((stf_reader.numInstsRead() < end_inst) && (stf_reader >> rec)) {
                    processRecord(rec, page_table, stf_reader.numInstsRead()) ;
                }
            }
            catch(const stf::EOFException&) {
            }

            return stf_reader.numInstsRead() - num_insts_read;
        }

        /**
         * \brief extract instcount instructions; and update TLB page table;
         * return number of instructions written;
         */
        uint64_t extractInsts(stf::STFReader& stf_reader,
                               stf::STFWriter& stf_writer,
                               const uint64_t instcount,
                               stf::STF_PTE &page_table) {
            stf::STFRecord::UniqueHandle rec;

            // Indicate the input file had been read in the past;
            // The trace_info and instruction initial info, such as
            // pc, physical pc are required to written in new output file.
            stf_reader.copyHeader(stf_writer);
            stf_writer.addTraceInfo(stf::TraceInfoRecord(stf::STF_GEN::STF_GEN_STF_MERGE,
                                                         stf::STF_CUR_VERSION_MAJOR,
                                                         stf::STF_CUR_VERSION_MINOR,
                                                         0,
                                                         "Merge trace generated by stf_merge"));
            stf_writer.setHeaderPC(stf_reader.getPC());
            if(stf_reader.getTraceFeatures()->hasFeature(stf::TRACE_FEATURES::STF_CONTAIN_PROCESS_ID)) {
                stf_writer << stf::ProcessIDExtRecord(inst_hw_tid_, inst_pid_, inst_tid_);
            }

            stf_writer.finalizeHeader();

            if(stf_reader.getTraceFeatures()->hasFeature(stf::TRACE_FEATURES::STF_CONTAIN_PTE)) {
                page_table.DumpPTEtoSTF(stf_writer);
            }

            const uint64_t init_count = stf_reader.numInstsRead();
            uint64_t count = 0;
            // Process trace records
            try {
                while ((count < instcount) && (stf_reader >> rec)) {
                    count = stf_reader.numInstsRead() - init_count;
                    processRecord(rec, page_table, count);
                    stf_writer << *rec;
                }
            }
            catch(const stf::EOFException&) {
            }

            std::cerr << "Output " << count << " instructions" << std::endl;
            return count;
        }
};
