// <STF_kernel_code_tracer> -*- HPP -*-

/**
 * \brief  This file defines the STF kernel code tracer class.
 *
 * This allows the user to easily generate a report of which parts of
 * a trace are kernel code or user code.
 *
 */

#pragma once

#include <deque>
#include <iostream>
#include <fstream>
#include <string>
#include "stf_decoder.hpp"
#include "stf_inst_reader.hpp"

/**
 * \namespace stf
 * \brief Defines all STF related classes
 */
namespace stf {
    /**
     * \class KernelCodeTracer
     * \brief This class keeps track of where kernel code occurs in a trace
     */
    class KernelCodeTracer {
    public:
        /**
         * \struct ExecBlock
         * \brief A record of a block of code that occurs in either user or kernel space
         */
        struct ExecBlock {
            /*< The address of the start of the block */
            uint64_t start_addr;
            /*< The address of the end of the block */
            uint64_t end_addr;

            // Defines if the block is user or kernel code, and if it is kernel
            // code, whether it is a syscall or not
            enum class BlockType {
                USER = 0,       /**< user code */

                USER_DUPLICATE, /**< user code, but PC is duplicated after the following kernel block */

                KERN_SVC,       /**< kernel code, system call */

                KERN_OTHER,     /**< kernel code, not system call */

                KERN_UNKNOWN,   /**< kernel code, unknown if system call or not */
                // It can be unknown if a block is system call code or not if the
                // trace began during the block's execution, so we never saw if
                // there was an SVC instruction

                UNDEFINED
            } type;

            /*< The number of instructions in the block */
            uint64_t count;

            /*< The trace index where the block begins */
            uint64_t idx;

            /**
             * \brief Constructor
             */
            ExecBlock(uint64_t start, uint64_t end, uint64_t count,
                    uint64_t idx, BlockType type):
                start_addr(start),
                end_addr(end),
                type(type),
                count(count),
                idx(idx)
            {};

            std::string TypeToString () const;
        };

        KernelCodeTracer(const std::string_view trace_filename, uint64_t start_inst, uint64_t end_inst) :
            inst_reader_(trace_filename),
            decoder_(inst_reader_.getInitialIEM())
        {
            inst_reader_.checkVersion();
            ConsumeInstReader(start_inst, end_inst);
        }

        /**
         * \brief Consume one instruction in the sequence
         *
         * Each instruction in the trace should be consumed in order
         */
        void ConsumeInst(const stf::STFInst& inst);

        /**
         * \brief Consume instructions from an STFInstReader
         *
         */
        void ConsumeInstReader(uint64_t start_inst, uint64_t end_inst);

        /**
         * \brief Print the kernel code statistics that have been gathered
         *
         * This includes total instructions, kernel instructions, and syscall
         * instructions. Optionally, print a timeline of all blocks of
         * user/kernel code including their index in the trace and length.
         */
        void Print(bool printTimeline = false) const;

        /**
         * \brief Write kernel trace in a manner meant for Maserati
         *
         * Format is <trace idx> <exec idx> <count> <type>
         * start_index is the first instruction we processed in the trace
         * start_delay is how many instructions are in the startup code
         */
        void writeKernTrace(std::string filename,
                std::string trace_filename, uint64_t start_index,
                uint64_t start_delay);

        /**
         * \brief Return the number of instructions that have been consumed
         */
        uint64_t GetTotalInsts() const { return totalInsts_; }

        /**
         * \brief Return the number of duplicate user insts that have been consumed
         */
        uint64_t GetUserDuplicateInsts() const { return userDuplicateInsts_; }

        /**
         * \brief Return the number of kernel instructions that have been consumed
         */
        uint64_t GetKernelInsts() const { return kernelInsts_; }

        /**
         * \brief Return the number of syscall instructions that have been consumed
         */
        uint64_t GetKernelSyscallInsts() const { return kernelSyscallInsts_; }

        /**
         * \brief Return a timeline of user/kernel code blocks
         */
        const std::deque<ExecBlock>& GetExecBlocks() const { return execBlocks_; }

        const ExecBlock& GetInstBlock(uint64_t index) const;

        ExecBlock::BlockType GetInstType(uint64_t index) const;

        bool instIsDuplicate(const stf::STFInst& inst) const;

    private:
        void InitFirstBlock(const stf::STFInst& inst);

        std::deque<ExecBlock::BlockType> blockTypeStack_;
        std::deque<ExecBlock> execBlocks_;
        STFInstReader inst_reader_;
        mutable STFDecoder decoder_;
        bool prevInstWasSyscall_ = false;
        bool prevInstWasEret_ = false;
        bool prevInstWasEvent_ = false;
        bool callsKernelCode_ = false;
        uint64_t totalInsts_ = 0;
        uint64_t userDuplicateInsts_ = 0;
        uint64_t kernelInsts_ = 0;
        uint64_t kernelSyscallInsts_ = 0;
    };

    bool KernelCodeTracer::instIsDuplicate(const stf::STFInst& inst) const {
        bool is_fault = false;

        for(const auto& event: inst.getEvents()) {
            if (event.isFault()) {
                is_fault = true;
                break;
            }
        }

        if(is_fault) {
            return false;
        }

        return (!inst.isSyscall()) && (!decoder_.decode(inst.opcode()).isBranch());

    }

    std::string KernelCodeTracer::ExecBlock::TypeToString() const {
        switch (type) {
            case KernelCodeTracer::ExecBlock::BlockType::USER:
                return "user";

            case KernelCodeTracer:: ExecBlock::BlockType::USER_DUPLICATE:
                return "user-duplicate";

            case KernelCodeTracer::ExecBlock::BlockType::KERN_SVC:
                return "kernel-syscall";

            case KernelCodeTracer::ExecBlock::BlockType::KERN_UNKNOWN:
                return "kernel-unknown";

            case KernelCodeTracer::ExecBlock::BlockType::KERN_OTHER:
                return "kernel";

            case KernelCodeTracer::ExecBlock::BlockType::UNDEFINED:
                stf_throw("ERROR: UNDEFINED stf::KernelCodeTracer::ExecBlock::BlockType");
        }
    }

    KernelCodeTracer::ExecBlock::BlockType KernelCodeTracer::GetInstType(uint64_t index) const {
        for (const KernelCodeTracer::ExecBlock& block : execBlocks_) {
            if (index >= block.idx && index <= (block.idx + block.count - 1)) {
                return block.type;
            }
        }

        return KernelCodeTracer::ExecBlock::BlockType::UNDEFINED;
    }

    static const KernelCodeTracer::ExecBlock EXEC_BLOCK_UNDEFINED(0, 0, 0, 0, KernelCodeTracer::ExecBlock::BlockType::UNDEFINED);

    const KernelCodeTracer::ExecBlock& KernelCodeTracer::GetInstBlock(uint64_t index) const {
        for (const KernelCodeTracer::ExecBlock& block : execBlocks_) {
            if (index >= block.idx && index <= (block.idx + block.count - 1)) {
                return block;
            }
        }


        return EXEC_BLOCK_UNDEFINED;
    }

    void KernelCodeTracer::InitFirstBlock(const stf::STFInst& inst) {
        ExecBlock::BlockType firstBlockType = ExecBlock::BlockType::USER;

        if (inst.isKernelCode()) {
            firstBlockType = ExecBlock::BlockType::KERN_UNKNOWN;
        } else if (instIsDuplicate(inst)) {
            firstBlockType = ExecBlock::BlockType::USER_DUPLICATE;
        }

        blockTypeStack_.emplace_back(firstBlockType);
        execBlocks_.emplace_back(inst.pc(), inst.pc(), 1, inst.index(), firstBlockType);
    }

    void KernelCodeTracer::ConsumeInstReader(uint64_t start_inst, uint64_t end_inst) {
        stf::STFInstReader::iterator it = inst_reader_.begin(start_inst);

        // Read instructions until we reach the end
        for (; it != inst_reader_.end(); it++) {
            const auto& inst = *it;

            // Quit when the end is reached
            if (end_inst && (inst.index() > end_inst)) {
                break;
            }

            // Skip instruction if invalid
            if (!inst.valid()) {
                continue;
            }

            ConsumeInst(inst);
        }
    }

    void KernelCodeTracer::ConsumeInst(const stf::STFInst& inst) {
        if (blockTypeStack_.size() > 0) {
            const bool instIsKernel = inst.isKernelCode();
            const bool execBlockIsKernel = (execBlocks_.back().type != ExecBlock::BlockType::USER &&
                                            execBlocks_.back().type != ExecBlock::BlockType::USER_DUPLICATE);

            // If current execution block is user space and this inst is in the
            // kernel, we need to start a new kernel execution block
            if (instIsKernel != execBlockIsKernel || (prevInstWasSyscall_ && execBlockIsKernel) || prevInstWasEvent_) {
                ExecBlock::BlockType newBlockType = ExecBlock::BlockType::KERN_OTHER;

                // In some rare cases, kernel code can call user code
                if (!instIsKernel) {
                    if (instIsDuplicate(inst)) {
                        newBlockType = ExecBlock::BlockType::USER_DUPLICATE;
                        userDuplicateInsts_++;
                    } else {
                        newBlockType = ExecBlock::BlockType::USER;
                    }

                } else if (prevInstWasSyscall_) {
                    newBlockType = ExecBlock::BlockType::KERN_SVC;
                }


                // Only push a new execution block if it is a different type than
                // the current one
                if (execBlocks_.back().type != newBlockType) {
                    execBlocks_.emplace_back(inst.pc(), inst.pc(), 1, inst.index(), newBlockType);
                    blockTypeStack_.emplace_back(newBlockType);
                } else {
                    execBlocks_.back().count++;
                    execBlocks_.back().end_addr = inst.pc();
                }

            // If the previous instruction was an ERET, we need to start a new
            // execution block for the event return
            } else if (prevInstWasEret_) {
                blockTypeStack_.pop_back();

                // If we don't have anything left on the stack, then the trace must
                // have begun inside a call and we need to initialize again
                if (blockTypeStack_.size() == 0) {
                    InitFirstBlock(inst);

                // If the current block type on the stack does not match the
                // instruction, then the return address for the event we just
                // returned from may have been overwritten. If so, we need to
                // clear the stack since it is now invalid.
                } else if (instIsKernel == ((blockTypeStack_.back() == ExecBlock::BlockType::USER) ||
                                            (blockTypeStack_.back() == ExecBlock::BlockType::USER_DUPLICATE))) {
                    blockTypeStack_.clear();
                    InitFirstBlock(inst);

                // Otherwise, the block type left on the stack is used for the
                // next execution block
                } else if (execBlocks_.back().type != blockTypeStack_.back()) {
                    execBlocks_.emplace_back(inst.pc(), inst.pc(), 1, inst.index(), blockTypeStack_.back());
                } else {
                    std::cerr << "ERROR: previous inst was ERET, but no ExecBlock type change occurred" << std::endl;
                    exit(1);
                }

            } else if ((blockTypeStack_.back() == ExecBlock::BlockType::USER) && instIsDuplicate(inst)) {
                execBlocks_.emplace_back(inst.pc(), inst.pc(), 1, inst.index(), ExecBlock::BlockType::USER_DUPLICATE);
                blockTypeStack_.emplace_back(ExecBlock::BlockType::USER_DUPLICATE);
                userDuplicateInsts_++;

            } else if ((blockTypeStack_.back() == ExecBlock::BlockType::USER_DUPLICATE) && !instIsDuplicate(inst)) {
                execBlocks_.emplace_back(inst.pc(), inst.pc(), 1, inst.index(), ExecBlock::BlockType::USER);
                blockTypeStack_.emplace_back(ExecBlock::BlockType::USER);


            // Otherwise, we remain in the same execution block
            } else {
                execBlocks_.back().count++;
                execBlocks_.back().end_addr = inst.pc();
            }

        } else {
            // This is the first instruction, so need to initialize
            InitFirstBlock(inst);
        }

        // Keep track if last instruction was a syscall, ERET, or had events
        decoder_.decode(inst.opcode());
        prevInstWasSyscall_ = inst.isSyscall();
        prevInstWasEret_ = decoder_.isExceptionReturn();
        prevInstWasEvent_ = !inst.getEvents().empty();

        // Keep track of the number of instructions encountered in kernel space,
        // syscalls, and total
        totalInsts_++;

        if (inst.isKernelCode()) {
            kernelInsts_++;

            if (blockTypeStack_.back() == ExecBlock::BlockType::KERN_SVC) {
                kernelSyscallInsts_++;
            }
        }

        // In case the trace did not capture kernel code, we still want to know if
        // there are any calls to kernel code while the workload runs
        if (prevInstWasSyscall_ || prevInstWasEvent_) {
            callsKernelCode_ = true;
        }
    }

    void KernelCodeTracer::Print(bool printTimeline) const {
        if (printTimeline) {
            std::cout << "idx\tinsts\tuser/kernel" << std::endl;

            for (const ExecBlock& execBlock : GetExecBlocks()) {
                std::cout << execBlock.idx << "\t"
                    << execBlock.count << "\t";

                std::cout << execBlock.TypeToString();

                std::cout << std::endl;
            }
        }

        float kernel_ratio = float(kernelInsts_) / float(totalInsts_);
        float kernel_percent = 100 * kernel_ratio;

        float kernel_syscall_ratio = float(kernelSyscallInsts_) / float(totalInsts_);
        float kernel_syscall_percent = 100 * kernel_syscall_ratio;

        std::cout << "total_insts: " << totalInsts_ << std::endl;
        std::cout << "user_duplicate_insts: " << userDuplicateInsts_ << std::endl;
        std::cout << "kernel_insts: " << kernelInsts_ << std::endl;
        std::cout << "kernel_syscall_insts: " << kernelSyscallInsts_ << std::endl;
        std::cout << "kernel_percent: " << kernel_percent << std::endl;
        std::cout << "kernel_syscall_percent: " << kernel_syscall_percent << std::endl;

        if (kernelInsts_ == 0 && callsKernelCode_) {
            std::cerr << "INFO: Calls kernel code with either SVC/EVT, "
                << "but kernel code was not recorded in the trace."
                << std::endl;
        }
    }

    void KernelCodeTracer::writeKernTrace(std::string filename,
            std::string trace_filename, uint64_t start_index,
            uint64_t start_delay)
    {
        std::ofstream kern_file;
        kern_file.open(filename);

        kern_file << "tracefile: " << trace_filename << std::endl;
        kern_file << "trace\texec" << std::endl;
        kern_file << "idx\tidx\tinsts\tuser/kernel" << std::endl;

        for (const auto& execBlock : GetExecBlocks()) {
            if ((execBlock.type == ExecBlock::BlockType::USER) ||
                (execBlock.type == ExecBlock::BlockType::USER_DUPLICATE)) {
                continue;
            }

            kern_file << execBlock.idx << "\t"
                << execBlock.idx - start_index + start_delay << "\t"
                << execBlock.count << "\t";

            kern_file << execBlock.TypeToString();

            kern_file << std::endl;
        }

        kern_file.close();
    }
} // end namespace stf
