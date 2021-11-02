// <stf_diff> -*- C++ -*-

/**
 * \brief  This tool compares two traces
 *
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "dtl/dtl.hpp"
#pragma GCC diagnostic pop

#include "stf_diff.hpp"

#include "print_utils.hpp"
#include "format_utils.hpp"
#include "stf_inst_reader.hpp"
#include "stf_decoder.hpp"

std::ostream& operator<<(std::ostream& os, const STFDiffInst& inst) {
    /*
    if (inst.physPC_ == INVALID_PHYS_ADDR)
        snprintf(buf, 100, "%ld:\t%#016lx", inst.index, inst.pc);
    else
        snprintf(buf, 100, "%ld:\t%#016lx:%#010lx:", inst.index, inst.pc,
                inst.physPC_);
    */

    stf::format_utils::formatDec(os, inst.index_);
    os << ":\t";
    stf::format_utils::formatHex(os, inst.pc_);
    os << " : ";
    stf::format_utils::formatHex(os, inst.opcode_);
    os << ' ';

    inst.dis_->printDisassembly(os, inst.pc_, inst.opcode_);

    if (!inst.mem_accesses_.empty()) {
        /*if (inst.mem_accesses[0].paddr == INVALID_PHYS_ADDR)
            snprintf(buf, 100, " MEM %#016lx: [",
                    inst.mem_accesses[0].addr);
        else
            snprintf(buf, 100, " MEM %#016lx:%#010lx: [",
                    inst.mem_accesses[0].addr,
                    inst.mem_accesses[0].paddr);*/
        os << " MEM ";
        stf::format_utils::formatHex(os, inst.mem_accesses_.front().getAddress());
        os << ": [";

        for (const auto &mit : inst.mem_accesses_) {
            os << ' ';
            stf::format_utils::formatHex(os, mit.getData());
        }

        os << " ]";
    }

    for (const auto &rit : inst.operands_) {
        // If there are state records, there should (generally) be a record for every single register in the machine.
        // Space them out to make the output more readable.
        if(STF_EXPECT_FALSE(rit.getType() == stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE)) {
            os << std::endl;
        }

        if(STF_EXPECT_FALSE(rit.isVector())) {
            std::ostringstream ss;
            ss << "   " << rit.getLabel() << ": " << rit.getReg() << " : ";
            const size_t padding = static_cast<size_t>(ss.tellp());
            os << ss.str();
            stf::format_utils::formatVector(os, rit.getVectorData(), rit.getVLen(), padding, false);
        }
        else {
            os << "   " << rit.getLabel() << ": " << rit.getReg() << " : ";
            stf::format_utils::formatHex(os, rit.getScalarData());
        }
    }

    return os;
}

auto getBeginIterator(const uint64_t start, const bool diff_markpointed_region, const bool diff_tracepointed_region, stf::STFInstReader& reader) {
    auto it = std::next(reader.begin(), static_cast<ssize_t>(start) - 1);

    if(diff_markpointed_region || diff_tracepointed_region) {
        stf::STFDecoder decoder(reader.getInitialIEM());
        while(it != reader.end()) {
            decoder.decode(it->opcode());
            ++it;
            if((diff_markpointed_region && decoder.isMarkpoint()) || (diff_tracepointed_region && decoder.isTracepoint())) {
                break;
            }
        }
    }

    return it;
}

inline bool isSC(stf::STFDecoder& decoder, const stf::STFInst& inst) {
    decoder.decode(inst.opcode());
    return decoder.isAtomic() && decoder.isStore();
}

inline bool isFailedSC(stf::STFDecoder& decoder, const stf::STFInst& inst) {
    if(STF_EXPECT_FALSE(isSC(decoder, inst))) {
        bool inst_is_failed_sc = true;
        for(const auto& m: inst.getMemoryAccesses()) {
            if(m.getType() == stf::INST_MEM_ACCESS::WRITE) {
                inst_is_failed_sc = false;
                break;
            }
        }

        return inst_is_failed_sc;
    }

    return false;
}

inline void advanceToCompletedSC(stf::STFDecoder& decoder,
                                 stf::STFInstReader::iterator& it,
                                 const stf::STFInstReader::iterator& end_it) {
    do {
        ++it;
        if(STF_EXPECT_FALSE(it == end_it)) {
            break;
        }
    }
    while(STF_EXPECT_TRUE(!isSC(decoder, *it) || isFailedSC(decoder, *it)));
}

// Given two traces (and some config info), report the first n differences
// between the two. n == config.diff_count.
int streamingDiff(const STFDiffConfig &config,
                  const std::string &trace1,
                  const std::string &trace2) {
    // Open stf trace reader
    stf::STFInstReader rdr1(trace1, config.ignore_kernel);
    stf::STFInstReader rdr2(trace2, config.ignore_kernel);

    auto reader1 = getBeginIterator(config.start1, config.diff_markpointed_region, config.diff_tracepointed_region, rdr1);
    auto reader2 = getBeginIterator(config.start2, config.diff_markpointed_region, config.diff_tracepointed_region, rdr2);

    uint64_t count = 0;
    uint64_t diff_count = 0;

    const auto inst_set = rdr1.getISA();
    stf_assert(inst_set == rdr2.getISA(), "Traces must have the same instruction set in order to be compared!");

    const auto iem = rdr1.getInitialIEM();
    stf_assert(iem == rdr2.getInitialIEM(), "Traces must have the same instruction encoding in order to be compared!");

    stf::STFDecoder decoder(iem);
    stf::Disassembler dis(inst_set, iem, config.use_aliases);

    const bool spike_lr_sc_workaround = config.workarounds.at("spike_lr_sc");

    while ((!config.length) || (count < config.length)) {
        if (diff_count >= config.diff_count) {
            break;
        }

        if (spike_lr_sc_workaround && reader1 != rdr1.end() && reader2 != rdr2.end()) {
            const bool inst1_was_failed_sc = isFailedSC(decoder, *reader1);
            const bool inst2_was_failed_sc = isFailedSC(decoder, *reader2);

            if(STF_EXPECT_FALSE(inst1_was_failed_sc != inst2_was_failed_sc)) {
                if(inst1_was_failed_sc) {
                    advanceToCompletedSC(decoder, reader1, rdr1.end());
                }
                else {
                    advanceToCompletedSC(decoder, reader2, rdr2.end());
                }
            }
        }

        if(reader1 == rdr1.end()) {
            if(reader2 == rdr2.end()) {
                break;
            }

            // Print out the stuff from trace 2
            diff_count++;
            if (!config.only_count) {
                const auto& inst2 = *reader2;
                std::cout << "+ "
                          << STFDiffInst(inst2,
                                         config,
                                         &dis)
                          << std::endl;
            }
            reader2++;
            count++;
            continue;
        }

        // Check if the second trace ended early
        if (reader2 == rdr2.end()) {
            diff_count++;
            if (!config.only_count) {
                const auto& inst1 = *reader1;
                std::cout << "- "
                          << STFDiffInst(inst1,
                                         config,
                                         &dis)
                          << std::endl;
            }
            reader1++;
            count++;
            continue;
        }

        const auto& inst1 = *reader1;
        const auto& inst2 = *reader2;

        STFDiffInst diff1(inst1,
                          config,
                          &dis);
        STFDiffInst diff2(inst2,
                          config,
                          &dis);

        if (diff1 != diff2) {
            diff_count++;
            if (!config.only_count) {
                std::cout << "- " << diff1 << std::endl;
                std::cout << "+ " << diff2 << std::endl;
            }
        }

        reader1++;
        reader2++;
        count++;
    }

    if (config.only_count) {
        std::cout << diff_count << " different instructions" << std::endl;
    }

    return !!diff_count;
}

void extractInstructions(const std::string &trace,
                         DiffInstVec &vec,
                         const uint64_t start,
                         const STFDiffConfig &config) {
    // Open stf trace reader
    stf::STFInstReader rdr(trace, config.ignore_kernel);
    stf::Disassembler dis(rdr.getISA(), rdr.getInitialIEM(), config.use_aliases);
    auto reader = getBeginIterator(start, config.diff_markpointed_region, config.diff_tracepointed_region, rdr);

    uint64_t count = 0;
    while (reader != rdr.end()) {
        const auto& inst = *reader;

        vec.emplace_back(inst,
                         config,
                         &dis);

        reader++;
        count++;

        // length of 0 means to keep going till we hit the end
        if (config.length && (count >= config.length)) {
            break;
        }
    }
}

int main (int argc, char **argv) {
    int ret = 0;
    try {
        const STFDiffConfig config(argc, argv);

        if (config.unified_diff) {
            DiffInstVec instVec1;
            DiffInstVec instVec2;

            extractInstructions(config.trace1,
                                instVec1,
                                config.start1,
                                config);
            extractInstructions(config.trace2,
                                instVec2,
                                config.start2,
                                config);

            std::cerr << "Now diffing " << instVec1.size() << '(' << instVec2.size() << ") instructions" << std::endl;

            dtl::Diff<STFDiffInst, DiffInstVec> d(instVec1, instVec2);

            d.onHuge();

            d.compose();

            d.composeUnifiedHunks();

            d.printUnifiedFormat();
        }
        else {
            ret = streamingDiff(config, config.trace1, config.trace2);
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return ret;
}
