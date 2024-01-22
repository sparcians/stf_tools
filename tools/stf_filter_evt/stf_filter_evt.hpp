#pragma once

#include <set>
#include <vector>

#include "stf_decoder.hpp"
#include "stf_inst.hpp"
#include "stf_reg_state.hpp"

class FilteredSTFInst : public stf::STFInst {
    private:
        bool branch_target_overridden_ = false;
        bool branch_target_invalidated_ = false;
        bool keep_events_ = true;
        bool reg_state_overridden_ = false;
        stf::STFRegState reg_state_;

        static inline bool isEventDescriptor_(const stf::descriptors::internal::Descriptor desc) {
            return desc == stf::descriptors::internal::Descriptor::STF_EVENT ||
                   desc == stf::descriptors::internal::Descriptor::STF_EVENT_PC_TARGET;
        }

        static inline bool descriptorHasBeenSkipped_(const stf::descriptors::internal::Descriptor cur_desc,
                                                     const stf::descriptors::internal::Descriptor desc_to_check) {
            return stf::descriptors::conversion::toEncoded(cur_desc) >= stf::descriptors::conversion::toEncoded(desc_to_check);
        }

    public:
        FilteredSTFInst(const stf::STFInstReader& reader) :
            reg_state_(reader.getISA(), reader.getInitialIEM())
        {
        }

        /**
         * \brief Write all records in this instruction to STFWriter
         */
        void write(stf::STFWriter& stf_writer) const {
            bool reg_state_written = false;
            bool branch_target_written = false;
            for (const auto& vec_pair: orig_records_.sorted()) {
                if(STF_EXPECT_FALSE(!branch_target_written && descriptorHasBeenSkipped_(vec_pair.first, stf::descriptors::internal::Descriptor::STF_INST_PC_TARGET))) {
                    if(STF_EXPECT_FALSE(branch_target_overridden_)) {
                        stf_writer << stf::InstPCTargetRecord(branch_target_);
                        branch_target_written = true;
                        if(STF_EXPECT_FALSE(vec_pair.first == stf::descriptors::internal::Descriptor::STF_INST_PC_TARGET)) {
                            continue;
                        }
                    }
                    if(STF_EXPECT_FALSE(branch_target_invalidated_)) {
                        branch_target_written = true;
                        if(STF_EXPECT_FALSE(vec_pair.first == stf::descriptors::internal::Descriptor::STF_INST_PC_TARGET)) {
                            continue;
                        }
                    }
                }
                if(STF_EXPECT_FALSE(!keep_events_)) {
                    if(STF_EXPECT_FALSE(isEventDescriptor_(vec_pair.first))) {
                        continue;
                    }
                }
                if(STF_EXPECT_FALSE(reg_state_overridden_ && !reg_state_written)) {
                    if(STF_EXPECT_FALSE(descriptorHasBeenSkipped_(vec_pair.first, stf::descriptors::internal::Descriptor::STF_INST_REG))) {
                        reg_state_.writeRegState(stf_writer);
                        reg_state_written = true;
                        if(vec_pair.first == stf::descriptors::internal::Descriptor::STF_INST_REG) {
                            for(const auto& record: vec_pair.second) {
                                if(STF_EXPECT_FALSE(record->as<stf::InstRegRecord>().getOperandType() == stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE)) {
                                    continue;
                                }
                                stf_writer << *record;
                            }
                            continue;
                        }
                    }
                }

                for(const auto& record: vec_pair.second) {
                    stf_writer << *record;
                }
            }
        }

        void invalidateBranchTarget() {
            branch_target_invalidated_ = true;
        }

        void overrideBranchTarget(const uint64_t new_target) {
            branch_target_overridden_ = true;
            branch_target_ = new_target;
        }

        void overrideRegState(const stf::STFRegState& reg_state) {
            reg_state_overridden_ = true;
            reg_state_ = reg_state;
        }

        void keepEvents(const bool keep_events) {
            keep_events_ = keep_events;
        }

        void reset() {
            branch_target_invalidated_ = false;
            branch_target_overridden_ = false;
            reg_state_overridden_ = true;

            stf::STFInst::reset_();
        }

        FilteredSTFInst& operator=(const STFInst& rhs) {
            stf::STFInst::operator=(rhs);

            branch_target_invalidated_ = false;
            branch_target_overridden_ = false;
            reg_state_overridden_ = true;

            return *this;
        }
};

struct STFFilterConfig {
    static constexpr uint64_t DEFAULT_KERNEL_ADDR = 0xffffff0000000000;
    std::string trace_filename;
    uint64_t startInst = 0;
    uint64_t endInst = 0;
    uint64_t kernelStartAddr = DEFAULT_KERNEL_ADDR;
    bool filterKernel=false;
    bool filterSyscalls=false;
    bool verbose = false;
    using EventSet = std::set<uint32_t>;
    EventSet eset;
    std::string output_filename = "-";	// By default, go to stdout.
};

class STFEventFilter {
    private:
        using PTEList = std::vector<stf::PageTableWalkRecord>;

        PTEList filtered_pte_;

        // This keeps the previous instruction state;
        FilteredSTFInst  prev_inst_;
        stf::STFDecoder decoder_;

    public:
        STFEventFilter(const stf::STFInstReader& reader) :
            prev_inst_(reader),
            decoder_(reader.getInitialIEM())
        {
        }

        void writeLastInst(stf::STFWriter& stf_writer, bool keep_event) {
            prev_inst_.invalidateBranchTarget();
            prev_inst_.keepEvents(keep_event);
            prev_inst_.write(stf_writer);
        }

        bool writeInst(stf::STFWriter& stf_writer,
                       const stf::STFInst& inst,
                       const bool keep_event,
                       stf::STFRegState& reg_state,
                       const bool verbose) {
            static bool first_inst = true;
            // before write current inst, complete the previous branch target(if necessary) and inst opcode;
            bool bBranch = false;
            if (STF_EXPECT_TRUE(!first_inst)) {
                if (prev_inst_.branchTarget()) {
                    bBranch = true;
                    if (prev_inst_.branchTarget() != inst.pc()) {
                        prev_inst_.overrideBranchTarget(inst.pc());
                        if (verbose) {
                            std::cerr << prev_inst_.index() << ", 0x";
                            stf::format_utils::formatHex(std::cerr, prev_inst_.pc());
                            std::cerr << " had PC target (";
                            stf::format_utils::formatHex(std::cerr, prev_inst_.branchTarget());
                            std::cerr << "), adjust to ";
                            stf::format_utils::formatHex(std::cerr, inst.pc());
                            std::cerr << '.' << std::endl;
                        }
                    }
                } else {
                    // previous does not have branch target;
                    // now need to decode it to check if the instruction is unconditional branch;
                    decoder_.decode(prev_inst_.opcode());
                    if(decoder_.isBranch()) {
                        if(decoder_.isConditional()) {
                            if ((prev_inst_.pc() + prev_inst_.opcodeSize()) != inst.pc()) {
                                prev_inst_.overrideBranchTarget(inst.pc());
                            }
                        }
                        else {
                            prev_inst_.overrideBranchTarget(inst.pc());
                        }
                        bBranch = true;
                    }
                }

                // if current instruction has register state dump; write them into trace;
                if (!inst.getRegisterStates().empty()) {
                    prev_inst_.overrideRegState(reg_state);
                }

                for(const auto& op: inst.getDestOperands()) {
                    reg_state.regStateUpdate(op.getRecord());
                }

                // write previous instruction
                prev_inst_.write(stf_writer);

                if (!bBranch) {
                    if ((prev_inst_.pc() + prev_inst_.opcodeSize()) != inst.pc()) {
                        stf_writer << stf::ForcePCRecord(inst.pc());
                    }
                }

            } else {
                stf_writer.setHeaderPC(inst.pc());
                stf_writer.finalizeHeader();
                first_inst = false;
            }

            for (const auto& rec: filtered_pte_) {
                stf_writer << rec;
            }
            filtered_pte_.clear();

            // don't write opcode yet, in case IRQ on branch instr.
            // wait for next instruction ready, to check branch target before writing opcode;
            prev_inst_ = inst;
            prev_inst_.keepEvents(keep_event);
            return true;
        }

        void appendFilteredPTE(const stf::STFInst& inst) {
            for(const auto& pte: inst.getEmbeddedPTEs()) {
                filtered_pte_.emplace_back(pte->as<stf::PageTableWalkRecord>());
            }
        }

        stf::STFDecoder& getDecoder() {
            return decoder_;
        }
};
