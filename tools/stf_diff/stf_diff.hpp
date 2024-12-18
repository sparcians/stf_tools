#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "disassembler.hpp"
#include "command_line_parser.hpp"
#include "stf_inst.hpp"
#include "stf_vlen.hpp"
#include "util.hpp"

class STFDiffConfig {
    public:
        std::string trace1;
        std::string trace2;
        uint64_t start1 = 1;
        uint64_t start2 = 1;
        uint64_t length = 0;
        unsigned int diff_count = 1;
        bool ignore_kernel = false;
        bool ignore_addresses = false;
        bool diff_memory= false;
        bool diff_physical_data = false;
        bool diff_physical_pc = false;
        bool diff_registers = false;
        bool diff_dest_registers = false;
        bool diff_state_registers = false;
        bool unified_diff = false;
        bool only_count = false;
        bool use_aliases = false;
        bool diff_markpointed_region = false;
        bool diff_tracepointed_region = false;
        std::map<std::string_view, bool> workarounds {
            {"spike_lr_sc", false}
        };

        STFDiffConfig(int argc, char **argv) {
            trace_tools::CommandLineParser parser("stf_diff");
            parser.addFlag('1', "N", "start diff on Nth instruction of trace1");
            parser.addFlag('2', "N", "start diff on Nth instruction of trace2");
            parser.addFlag('l', "len", "run diff for <len> instructions");
            parser.addFlag('k', "ignore kernel instructions");
            parser.addFlag('A', "ignore addresses");
            parser.addFlag('M', "compare memory records");
            parser.addFlag('p', "compare all physical addresses");
            parser.addFlag('P', "only compare data physical addresses");
            parser.addFlag('R', "compare register records");
            parser.addFlag('D', "compare destination register records");
            parser.addFlag('S', "compare register state records");
            parser.addFlag('c', "N", "Exit after the Nth difference");
            parser.addFlag('C', "Just report the number of differences");
            parser.addFlag('u', "run unified diff");
            parser.addFlag('a', "use register aliases in disassembly");
            parser.addFlag('m', "begin diff after first markpoint");
            parser.addFlag('t', "begin diff after first tracepoint");
            parser.addMultiFlag('W', "workaround", "enable specified workaround");
            parser.addPositionalArgument("trace1", "first STF trace to compare");
            parser.addPositionalArgument("trace2", "second STF trace to compare");
            parser.appendHelpText("Workarounds:\n"
                                  "    spike_lr_sc: If a mismatch occurs due to a failed LR/SC pair, realign the traces and continue\n");

            parser.setMutuallyExclusive('A', 'p');
            parser.setMutuallyExclusive('A', 'P');
            parser.setMutuallyExclusive('m', 't');
            parser.setMutuallyExclusive('R', 'D');

            parser.parseArguments(argc, argv);

            parser.getArgumentValue('1', start1);
            parser.getArgumentValue('2', start2);
            parser.getArgumentValue('l', length);
            parser.getArgumentValue('c', diff_count);
            ignore_kernel = parser.hasArgument('k');
            ignore_addresses = parser.hasArgument('A');
            diff_memory = parser.hasArgument('M');
            diff_physical_pc = parser.hasArgument('p');
            diff_physical_data = diff_physical_pc || parser.hasArgument('P');
            diff_registers = parser.hasArgument('R');
            diff_dest_registers = parser.hasArgument('D');
            diff_state_registers = parser.hasArgument('D');
            unified_diff = parser.hasArgument('u');
            only_count = parser.hasArgument('C');
            use_aliases = parser.hasArgument('a');
            diff_markpointed_region = parser.hasArgument('m');
            diff_tracepointed_region = parser.hasArgument('t');

            for(const auto& workaround: parser.getMultipleValueArgument('W')) {
                if(auto it = workarounds.find(workaround); it != workarounds.end()) {
                    it->second = true;
                }
                else {
                    parser.raiseErrorWithHelp("Invalid workaround specified: " + workaround);
                }
            }

            parser.getPositionalArgument(0, trace1);
            parser.getPositionalArgument(1, trace2);

            parser.assertCondition(start1, "-1 parameter must be nonzero");
            parser.assertCondition(start2, "-2 parameter must be nonzero");
        }
};

class STFDiffInst {
    private:
        class MemAccess {
            private:
                using VectorType = boost::container::small_vector<uint64_t, 1>;
                const uint64_t addr_;
                const VectorType data_;

                MemAccess(const uint64_t addr, const stf::MemAccess::ContentValueView& data_view) :
                    addr_(addr),
                    data_(data_view.begin(), data_view.end())
                {
                }

            public:
                MemAccess(const stf::MemAccess& mem_access, bool ignore_addresses) :
                    MemAccess(ignore_addresses ? stf::page_utils::INVALID_PHYS_ADDR : mem_access.getAddress(), mem_access.getData())
                {
                }

                uint64_t getAddress() const {
                    return addr_;
                }

                const VectorType& getData() const {
                    return data_;
                }

                bool operator==(const MemAccess& rhs) const {
                    //return (addr_ == rhs.addr_) && (paddr_ == rhs.paddr_) && (data_ == rhs.data_);
                    return (addr_ == rhs.addr_) && (data_ == rhs.data_);
                }

                bool operator!=(const MemAccess& rhs) const {
                    return !(*this == rhs);
                }
        };

        class Operand {
            private:
                const stf::Registers::STF_REG reg_;
                const stf::Registers::STF_REG_OPERAND_TYPE type_;
                const stf::InstRegRecord::VectorType data_;
                const std::string_view label_;
                const stf::vlen_t vlen_;

            public:
                explicit Operand(const stf::Operand& op) :
                    reg_(op.getReg()),
                    type_(op.getType()),
                    data_(op.isVector() ? op.getVectorValue() : stf::InstRegRecord::VectorType(1, op.getScalarValue())),
                    label_(op.getLabel()),
                    vlen_(op.getVLen())
                {
                }

                stf::Registers::STF_REG getReg() const {
                    return reg_;
                }

                stf::Registers::STF_REG_OPERAND_TYPE getType() const {
                    return type_;
                }

                const stf::InstRegRecord::VectorType& getVectorData() const {
                    stf_assert(isVector(), "Attempted to get vector data from a scalar operand");
                    return data_;
                }

                uint64_t getScalarData() const {
                    stf_assert(!isVector(), "Attempted to get scalar data from a vector operand");
                    return data_.front();
                }

                const std::string_view& getLabel() const {
                    return label_;
                }

                bool operator==(const Operand& rhs) const {
                    return (type_ == rhs.type_) && (reg_ == rhs.reg_) && (data_ == rhs.data_);
                }

                bool operator!=(const Operand& rhs) const {
                    return !(*this == rhs);
                }

                bool isVector() const {
                    return data_.size() > 1;
                }

                stf::vlen_t getVLen() const {
                    return vlen_;
                }
        };

        uint64_t pc_;
        //uint64_t physPC_;
        uint32_t opcode_;
        uint64_t index_;
        std::vector<MemAccess> mem_accesses_;
        std::vector<Operand> operands_;
        const stf::Disassembler* dis_ = nullptr;

        template<typename OperandVectorType>
        inline void addOperands_(const OperandVectorType& operands) {
            operands_.reserve(operands.size());
            for(const auto& op: operands) {
                operands_.emplace_back(op);
            }
        }

    public:
        STFDiffInst(const stf::STFInst& inst,
                    const STFDiffConfig& config,
                    const stf::Disassembler* dis) :
            pc_(config.ignore_addresses ? stf::page_utils::INVALID_PHYS_ADDR : inst.pc()),
            opcode_(inst.opcode()),
            index_(inst.index()),
            dis_(dis)
        {
            //physPC_ = pPC ? inst->physPc() : INVALID_PHYS_ADDR;
            // Ignore data on syscalls
            if (config.diff_memory && !inst.isSyscall()) {
                for(const auto& mit: inst.getMemoryAccesses()) {
                    mem_accesses_.emplace_back(mit, config.ignore_addresses);
                }
            }

            // If registers are on, check those
            if (config.diff_registers) {
                addOperands_(inst.getOperands());
            }
            else if(config.diff_dest_registers) {
                addOperands_(inst.getDestOperands());
            }

            if(config.diff_state_registers) {
                addOperands_(inst.getRegisterStates());
            }
        }

        STFDiffInst(const STFDiffInst& rhs) = default;
        STFDiffInst(STFDiffInst&& rhs) = default;

        STFDiffInst& operator=(STFDiffInst&& rhs) noexcept {
            if(this == &rhs) {
                return *this;
            }

            pc_ = std::move(rhs.pc_);
            opcode_ = std::move(rhs.opcode_);
            index_ = std::move(rhs.index_);
            mem_accesses_ = std::move(rhs.mem_accesses_);
            operands_ = std::move(rhs.operands_);
            dis_ = std::move(rhs.dis_);

            return *this;
        }

        STFDiffInst& operator=(const STFDiffInst& rhs) {
            if(this == &rhs) {
                return *this;
            }

            pc_ = rhs.pc_;
            opcode_ = rhs.opcode_;
            index_ = rhs.index_;
            mem_accesses_.clear();
            std::copy(rhs.mem_accesses_.begin(), rhs.mem_accesses_.end(), std::back_inserter(mem_accesses_));
            operands_.clear();
            std::copy(rhs.operands_.begin(), rhs.operands_.end(), std::back_inserter(operands_));
            dis_ = rhs.dis_;

            return *this;
        }

        bool operator==(const STFDiffInst &other) const {
            //if ((pc != other.pc) || (physPC_ != other.physPC_) || (opcode != other.opcode)) {
            if ((pc_ != other.pc_) || (opcode_ != other.opcode_)) {
                return false;
            }

            if (mem_accesses_.size() != other.mem_accesses_.size()) {
                return false;
            }

            if (!mem_accesses_.empty()) {
                auto m2 = other.mem_accesses_.cbegin();
                for (const auto &m1 : mem_accesses_) {
                    if (m2 == other.mem_accesses_.end()) {
                        return false;
                    }

                    if (m1 != *m2) {
                        return false;
                    }

                    m2++;
                }

                if (m2 != other.mem_accesses_.end()) {
                    return false;
                }
            }

            if (operands_.size() != other.operands_.size()) {
                return false;
            }

            if (!operands_.empty()) {
                auto r2 = other.operands_.cbegin();
                for (const auto &r1 : operands_) {
                    if (r2 == other.operands_.end()) {
                        return false;
                    }

                    if (r1 != *r2) {
                        return false;
                    }

                    r2++;
                }

                if (r2 != other.operands_.end()) {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(STFDiffInst &other) {
            return !(*this == other);
        }

        friend std::ostream& operator<<(std::ostream& os, const STFDiffInst& inst);
};

using DiffInstVec = std::vector<STFDiffInst>;
