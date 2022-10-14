#pragma once

#include <string>
#include <iostream>
#include <vector>
#include "stf_inst_reader.hpp"
#include "stf_pte.hpp"
#include "stf_record.hpp"
#include "stf_reg_state.hpp"
#include "stf_writer.hpp"

namespace stf {
    /**
     * \class STFFilter
     *
     * This parent class can be used to iterate through an input trace, filter
     * the instructions in any arbitrary way, and write the filtered
     * instructions to an output trace.
     *
     * The purpose of using this class is to provide any common code and logic
     * that any STF trace extracting/filtering application would need to use.
     * In the past, all of those applications would have their own code for
     * this, so we had a lot of duplication. Furthermore, many applications did
     * all of this in slightly different ways. Consolodating all the logic into
     * one class that every application uses will drastically cut down on tech
     * debt, making all the applications' code easier to understand and manage.
     *
     * How to use:
     *
     * Create a class that publicly inherits STFFilter. Make sure its
     * constructor calls a constructor of STFFilter. Override the filter()
     * method with whatever code you want. Look at STFExampleFilter below for an
     * exmple. Once you've instantiated your filter subclass, call the extract()
     * method to filter an input trace.
     */
    template<typename DerivedType>
    class STFFilter {
        public:
            /**
             * Use this constructor if you want to produce an output trace.
             *
             * \param stf_inst_reader object to read instructions from
             * \param stf_writer object to write instructions to
             */
            STFFilter(STFInstReader& stf_inst_reader,
                      std::shared_ptr<STFWriter> stf_writer,
                      const bool user_mode_only) :
                user_mode_only_(user_mode_only),
                stf_inst_reader_(stf_inst_reader),
                stf_writer_(std::move(stf_writer)),
                reg_state_(stf_inst_reader.getISA(), stf_inst_reader.getInitialIEM()),
                page_table_(nullptr, nullptr, true),
                in_user_code_(user_mode_only)
            {}

            /**
             * Use this constructor if you do not want to produce an output trace.
             *
             * \param stf_inst_reader object to read instructions from
             */
            STFFilter(STFInstReader& stf_inst_reader, const bool user_mode_only) :
                STFFilter(stf_inst_reader, nullptr, user_mode_only)
            {}

            /**
             * Iterate through the input trace, calling filter() on all instructions extracted.
             *
             * \param num_to_skip skip over this many instruction first, without extracting
             * \param num_to_extract extract this many instructions from the input trace
             * \param isa ISA for this trace
             * \param iem INST_IEM for this trace
             */
            void extract(const uint64_t num_to_skip, const uint64_t num_to_extract, const ISA isa, const INST_IEM iem) {
                if(stf_inst_reader_.getTraceFeatures()->hasFeature(TRACE_FEATURES::STF_CONTAIN_VEC)) {
                    reg_state_.resetArch(isa, iem);
                }
                if(stf_inst_reader_.getTraceFeatures()->hasFeature(TRACE_FEATURES::STF_CONTAIN_PTE)) {
                    dump_ptes_on_demand_ = true;
                }

                if(stf_writer_) {
                    stf_inst_reader_.copyHeader(*stf_writer_);
                }
                for (auto inst_it = stf_inst_reader_.begin(); (inst_it != stf_inst_reader_.end()) && (num_insts_extracted_ < num_to_extract); ++inst_it) {
                    const auto& inst = *inst_it;

                    if (num_insts_read_ >= num_to_skip) {
                        if (num_insts_extracted_ == 0 && num_to_skip > 0) {
                            if (stf_writer_) {
                                writeStartingRecords(inst);
                            }
                        }

                        is_fault_ = inst.isFault();

                        const auto& insts_to_write = static_cast<DerivedType*>(this)->filter(inst);

                        if (stf_writer_ && !insts_to_write.empty()) {
                            for (const auto& inst_to_write: insts_to_write) {
                                inst_to_write.write(*stf_writer_);
                                ++num_insts_written_;
                            }
                        }

                        ++num_insts_extracted_;

                    } else {
                        // If we skip this instruction, we may still need to copy some of its records
                        processSkippedInstRecords(inst);
                    }

                    ++num_insts_read_;

                    in_user_code_ = user_mode_only_ || (!inst.isChangeFromUserMode() && (in_user_code_ || inst.isChangeToUserMode()));
                }
                static_cast<const DerivedType*>(this)->finished();
            }


        protected:
            inline static const std::vector<stf::STFInst> EMPTY_INST_LIST_;

            /**
             * This function should be overridden by child classes for different
             * filtering purposes. This function is called on every instruction
             * extracted from the input trace, not including any skipped instructions
             * at the beginning of the trace.
             *
             * The parent class does not filter anything out--it just returns all the
             * instructions given to it.
             *
             * \param inst The next instruction read from the input trace
             *
             * \returns vector of instructions to write to the output trace
             */
            inline const std::vector<STFInst>& filter(const STFInst& inst) { return EMPTY_INST_LIST_; }

            /**
             * Sometimes a task needs to be performed at the end of an extraction. This
             * function can be overridden for this purpose.
             */
            void finished() const {
                std::cerr << "Read " << num_insts_read_ << " insts" << std::endl;
            }

            /**
             * Extracts any records that may be needed later from skipped instructions
             * \param inst Instruction that has been skipped
             */
            void processSkippedInstRecords(const STFInst& inst) {
                const auto& orig_records = inst.getOrigRecords();

                for(const auto& rec: orig_records.at(stf::descriptors::internal::Descriptor::STF_INST_REG)) {
                    const auto& reg_rec = rec->as<InstRegRecord>();
                    if ((reg_rec.getOperandType() == Registers::STF_REG_OPERAND_TYPE::REG_DEST) ||
                        (reg_rec.getOperandType() == Registers::STF_REG_OPERAND_TYPE::REG_STATE)) {
                        reg_state_.regStateUpdate(reg_rec);
                    }
                }

                for(const auto& walk_info: orig_records.at(stf::descriptors::internal::Descriptor::STF_PAGE_TABLE_WALK)) {
                    page_table_.UpdatePTE(inst.asid(), &walk_info->as<PageTableWalkRecord>());
                }

                for(const auto& rec: orig_records.at(stf::descriptors::internal::Descriptor::STF_COMMENT)) {
                    if (stf_writer_) {
                        stf_writer_->addHeaderComment(rec->as<CommentRecord>().getData());
                    }
                }

                /*for (auto it = inst.origRecordsBegin(); it != inst.origRecordsEnd(); ++it) {
                    for(const auto& rec: it->second) {
                        switch (rec->getId()) {
                            case stf::descriptors::internal::Descriptor::STF_INST_REG:
                                {
                                    const auto& reg_rec = rec->as<InstRegRecord>();
                                    if ((reg_rec.getOperandType() == Registers::STF_REG_OPERAND_TYPE::REG_DEST) ||
                                        (reg_rec.getOperandType() == Registers::STF_REG_OPERAND_TYPE::REG_STATE)) {
                                        reg_state_.regStateUpdate(reg_rec);
                                    }
                                }
                                break;

                            case stf::descriptors::internal::Descriptor::STF_PAGE_TABLE_WALK:
                                page_table_.UpdatePTE(inst.asid(),
                                                      std::static_pointer_cast<PageTableWalkRecord>(std::shared_ptr<stf::STFRecord>(rec->clone())));
                                break;

                            case stf::descriptors::internal::Descriptor::STF_COMMENT:
                                if (stf_writer_) {
                                    stf_writer_->addHeaderComment(rec->as<CommentRecord>().getData());
                                }
                                break;

                            default:
                                break;
                        }
                    }
                }*/
            }

            /**
             * Writes new STF header
             * \param inst Instruction that will be the first to be extracted
             */
            void writeStartingRecords(const STFInst& inst) {
                stf_writer_->setHeaderPC(inst.pc());

                if (stf_inst_reader_.getTraceFeatures()->hasFeature(TRACE_FEATURES::STF_CONTAIN_PROCESS_ID)) {
                    *stf_writer_ << ProcessIDExtRecord(inst.tgid(), inst.tid(), inst.asid());
                }

                stf_writer_->finalizeHeader();

                reg_state_.writeRegState(*stf_writer_);

                if (stf_inst_reader_.getTraceFeatures()->hasFeature(TRACE_FEATURES::STF_CONTAIN_PTE)) {
                    if(!dump_ptes_on_demand_) {
                        page_table_.DumpPTEtoSTF(*stf_writer_);

                    } else {
                        page_table_.ResetUsage();

                        //Output first PTE for first PC
                        page_table_.CheckAndDumpNewPTESingle(*stf_writer_, inst.asid(), inst.pc(), 1, 4);
                    }
                }
            }


            const bool user_mode_only_ = false; /**< If true, the reader will only return user-mode code */

            STFInstReader& stf_inst_reader_; /**< STFInstReader used to read instructions */
            std::shared_ptr<STFWriter> stf_writer_; /**< STFWriter used to write filtered records - may be nullptr if we aren't writing anything */
            STFRegState reg_state_; /**< Tracks register state */

            uint64_t num_insts_extracted_ = 0; /**< Counts number of instructions extracted */
            uint64_t num_insts_read_ = 0; /**< Counts number of instructions read */
            uint64_t num_insts_written_ = 0; /**< Counts number of instructions written */
            STF_PTE page_table_; /**< Tracks page table info */

            bool dump_ptes_on_demand_ = false; /**< If true, dumps PTEs inline with instructions */
            bool in_user_code_ = false; /**< If true, current instruction is user code */
            bool is_fault_ = false; /**< If true, current instruction is a fault */
    };
} // end namespace stf
