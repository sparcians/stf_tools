#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "Disassembler.hpp"
#include "command_line_parser.hpp"
#include "stf_inst.hpp"
#include "util.hpp"

struct STFDiffConfig {
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
    bool unified_diff = false;
    bool only_count = false;
    bool use_aliases = false;
    bool diff_markpointed_region = false;
    bool diff_tracepointed_region = false;

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
        parser.addFlag('c', "N", "Exit after the Nth difference");
        parser.addFlag('C', "Just report the number of differences");
        parser.addFlag('u', "run unified diff");
        parser.addFlag('a', "use register aliases in disassembly");
        parser.addFlag('m', "begin diff after first markpoint");
        parser.addFlag('t', "begin diff after first tracepoint");
        parser.addPositionalArgument("trace1", "first STF trace to compare");
        parser.addPositionalArgument("trace2", "second STF trace to compare");
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
        unified_diff = parser.hasArgument('u');
        only_count = parser.hasArgument('C');
        use_aliases = parser.hasArgument('a');
        diff_markpointed_region = parser.hasArgument('m');
        diff_tracepointed_region = parser.hasArgument('t');

        parser.getPositionalArgument(0, trace1);
        parser.getPositionalArgument(1, trace2);

        if (!start1) {
            parser.raiseErrorWithHelp("-1 parameter must be nonzero");
        }

        if (!start2) {
            parser.raiseErrorWithHelp("-2 parameter must be nonzero");
        }

        if (ignore_addresses && (diff_physical_data || diff_physical_pc)) {
            parser.raiseErrorWithHelp("-A and -p/-P parameters are mutually exclusive");
        }

        if(diff_markpointed_region && diff_tracepointed_region) {
            parser.raiseErrorWithHelp("-m and -t parameters are mutually exclusive");
        }
    }
};

class STFDiffInst {
    private:
        class MemAccess {
            private:
                const uint64_t addr_;
                const uint64_t data_;

            public:
                MemAccess(const stf::MemAccess& mem_access, bool ignore_addresses) :
                    addr_(ignore_addresses ? stf::page_utils::INVALID_PHYS_ADDR : mem_access.getAddress()),
                    data_(mem_access.getData())
                {
                }

                uint64_t getAddress() const {
                    return addr_;
                }

                uint64_t getData() const {
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
                const uint64_t data_;
                const std::string_view label_;

            public:
                explicit Operand(const stf::Operand& op) :
                    reg_(op.getReg()),
                    type_(op.getType()),
                    data_(op.getValue()),
                    label_(op.getLabel())
                {
                }

                stf::Registers::STF_REG getReg() const {
                    return reg_;
                }

                stf::Registers::STF_REG_OPERAND_TYPE getType() const {
                    return type_;
                }

                uint64_t getData() const {
                    return data_;
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
        };

        uint64_t pc_;
        //uint64_t physPC_;
        uint32_t opcode_;
        uint64_t index_;
        std::vector<MemAccess> mem_accesses_;
        std::vector<Operand> operands_;
        const stf::ISA isa_;
        const stf::Disassembler* dis_ = nullptr;

    public:
        STFDiffInst(const stf::STFInst& inst,
                    bool pData,
                    bool pPC,
                    bool diff_memory,
                    bool diff_registers,
                    bool ignore_addresses,
                    const stf::ISA isa,
                    const stf::Disassembler* dis) :
            pc_(ignore_addresses ? stf::page_utils::INVALID_PHYS_ADDR : inst.pc()),
            opcode_(inst.opcode()),
            index_(inst.index()),
            isa_(isa),
            dis_(dis)
        {
            //physPC_ = pPC ? inst->physPc() : INVALID_PHYS_ADDR;
            // Ignore data on syscalls
            if (diff_memory && !inst.isSyscall()) {
                for(const auto& mit: inst.getMemoryAccesses()) {
                    mem_accesses_.emplace_back(mit, ignore_addresses);
                }
            }

            // If registers are on, check those
            if (diff_registers) {
                for(const auto& rit: inst.getOperands()) {
                    operands_.emplace_back(rit);
                }
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
            const_cast<stf::ISA&>(isa_) = std::move(rhs.isa_);
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
            const_cast<stf::ISA&>(isa_) = rhs.isa_;
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
