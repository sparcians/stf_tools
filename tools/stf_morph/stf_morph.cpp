#include <string>
#include <unordered_map>
#include <vector>

#include "command_line_parser.hpp"

#include "stf_decoder.hpp"
#include "stf_inst_reader.hpp"
#include "stf_reg_state.hpp"
#include "stf_writer.hpp"
#include "stf_record_types.hpp"

class OpcodeMorph {
    public:
        class Op {
            private:
                uint32_t opcode_;
                mutable std::vector<stf::InstRegRecord> operands_;
                uint64_t ls_address_;
                uint16_t ls_size_;
                stf::INST_MEM_ACCESS ls_access_type_;
                size_t op_size_;

            public:
                Op(const uint32_t opcode,
                   std::vector<stf::InstRegRecord>&& operands,
                   const uint64_t ls_address,
                   const uint16_t ls_size,
                   const stf::INST_MEM_ACCESS ls_access_type,
                   const size_t op_size) :
                    opcode_(opcode),
                    operands_(std::move(operands)),
                    ls_address_(ls_address),
                    ls_access_type_(ls_access_type),
                    op_size_(op_size)
                {
                }

                size_t write(stf::STFWriter& writer, const stf::STFRegState& reg_state) const {
                    for(auto& op: operands_) {
                        uint64_t reg_data = 0;

                        // Grab the latest values for each operand
                        try {
                            reg_state.getRegValue(op.getReg(), reg_data);
                        }
                        catch(const stf::STFRegState::RegNotFoundException&) {
                            // Defaults to 0 if we haven't seen the register in the trace yet
                        }

                        op.setData(reg_data);
                        writer << op;
                    }

                    if(STF_EXPECT_FALSE(ls_access_type_ != stf::INST_MEM_ACCESS::INVALID)) {
                        writer << stf::InstMemAccessRecord(ls_address_,
                                                           ls_size_,
                                                           0,
                                                           ls_access_type_);
                        writer << stf::InstMemContentRecord(0);
                    }

                    if(op_size_ == 2) {
                        writer << stf::InstOpcode16Record(static_cast<uint16_t>(opcode_));
                    }
                    else {
                        writer << stf::InstOpcode32Record(opcode_);
                    }

                    return op_size_;
                }
        };

    private:
        size_t total_size_;
        std::vector<Op> opcodes_;

    public:
        inline void addOp(const uint32_t opcode,
                          std::vector<stf::InstRegRecord>&& operands,
                          const uint64_t ls_address,
                          const uint16_t ls_size,
                          const stf::INST_MEM_ACCESS ls_access_type,
                          const size_t op_size) {
            opcodes_.emplace_back(opcode,
                                  std::forward<std::vector<stf::InstRegRecord>>(operands),
                                  ls_address,
                                  ls_size,
                                  ls_access_type,
                                  op_size);
            total_size_ += op_size;
        }

        const auto& getOpcodes() const {
            return opcodes_;
        }

        auto getTotalSize() const {
            return total_size_;
        }
};

struct STFEditConfig {
    std::string trace;
    std::string output;
    std::unordered_map<uint64_t, OpcodeMorph> opcode_morphs;
    uint64_t start_inst = 0;
    uint64_t end_inst = 0;
};

static STFEditConfig parseCommandLine(int argc, char **argv) {
    STFEditConfig config;
    trace_tools::CommandLineParser parser("stf_morph");
    parser.addFlag('o', "trace", "output filename");
    parser.addFlag('s', "N", "start at instruction N");
    parser.addFlag('e', "M", "end at instruction M");
    parser.addFlag('A', "address", "assume all LS ops access the given address");
    parser.addFlag('S', "size", "assume all LS ops have the given size");
    parser.addMultiFlag('a',
                        "pc=opcode1[@addr1:size1][,opcode2[@addr2:size2],...]",
                        "morph instruction(s) starting at pc to specified opcode(s). "
                        "LS instructions can have target addresses and access sizes specified with `opcode@addr:size` syntax");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', config.output);
    parser.getArgumentValue('s', config.start_inst);
    parser.getArgumentValue('e', config.end_inst);

    if(STF_EXPECT_FALSE(config.output.empty())) {
        parser.raiseErrorWithHelp("No output file specified.");
    }

    const auto& opcode_morphs = parser.getMultipleValueArgument('a');

    stf::STFDecoder decoder;
    uint64_t global_address = 0;
    const bool has_global_address = parser.hasArgument('A');
    parser.getArgumentValue<uint64_t, 16>('A', global_address);

    uint16_t global_size = 0;
    const bool has_global_size = parser.hasArgument('S');
    parser.getArgumentValue('S', global_size);

    for(const auto& morph: opcode_morphs) {
        const auto eq_pos = morph.find('=');
        stf_assert(eq_pos != std::string::npos, "Invalid -a value specified: " << morph);

        const auto pc_str = morph.substr(0, eq_pos);
        const uint64_t pc = std::stoull(pc_str, 0, 16);
        std::stringstream opcode_stream(morph.substr(eq_pos + 1));

        try {
            std::string opcode_str;
            const auto result = config.opcode_morphs.try_emplace(pc);
            if(STF_EXPECT_FALSE(!result.second)) {
                parser.raiseErrorWithHelp("PC " + pc_str + " specified multiple times.");
            }

            const auto morph_it = result.first;
            while(getline(opcode_stream, opcode_str, ',')) {
                const auto at_pos = opcode_str.find('@');
                uint32_t opcode = 0;
                uint64_t address = 0;
                uint16_t size = 0;
                if(at_pos != std::string::npos) {
                    const auto colon_pos = opcode_str.find(':');
                    if(STF_EXPECT_FALSE(colon_pos == std::string::npos)) {
                        if(has_global_size) {
                            size = global_size;
                        }
                        else {
                            parser.raiseErrorWithHelp("Did not specify an access size for an LS op: " + opcode_str);
                        }
                    }
                    address = std::stoull(opcode_str.substr(at_pos + 1, colon_pos - at_pos - 1), 0, 16);
                    size = static_cast<uint16_t>(std::stoul(opcode_str.substr(colon_pos + 1)));
                    opcode = static_cast<uint32_t>(std::stoul(opcode_str.substr(0, at_pos), 0, 16));
                }
                else {
                    opcode = static_cast<uint32_t>(std::stoul(opcode_str, 0, 16));
                }
                decoder.decode(opcode);

                if(STF_EXPECT_FALSE(decoder.isBranch())) {
                    parser.raiseErrorWithHelp("Instructions cannot be replaced with branches. Opcode " + opcode_str + " is a branch.");
                }
                else if(STF_EXPECT_FALSE(at_pos != std::string::npos && !(decoder.isLoad() || decoder.isStore()))) {
                    parser.raiseErrorWithHelp("Specified a destination/source address for a non-LS op: " + opcode_str);
                }
                else if(STF_EXPECT_FALSE(at_pos == std::string::npos && (decoder.isLoad() || decoder.isStore()))) {
                    if(has_global_address) {
                        address = global_address;
                    }
                    else {
                        parser.raiseErrorWithHelp("Attempted to add a load/store op without specifying the destination/source address: " + opcode_str);
                    }
                }

                const size_t inst_size = decoder.isCompressed() ? 2 : 4;
                morph_it->second.addOp(opcode,
                                       decoder.getRegisterOperands(),
                                       address,
                                       size,
                                       decoder.getMemAccessType(),
                                       inst_size);
            }
        }
        catch(const stf::STFDecoder::InvalidInstException& e) {
            parser.raiseErrorWithHelp(std::string("Invalid opcode specified: ") + e.what());
        }
    }

    if(STF_EXPECT_FALSE(config.opcode_morphs.empty())) {
        parser.raiseErrorWithHelp("No modifications specified.");
    }

    parser.getPositionalArgument(0, config.trace);

    return config;
}

void updateInitialRegState(stf::STFRegState& reg_state, const stf::STFInstReader::iterator& it) {
    for(const auto& op: it->getRegisterStates()) {
        reg_state.regStateUpdate(op.getReg(), op.getValue());
    }

    for(const auto& op: it->getSourceOperands()) {
        reg_state.regStateUpdate(op.getReg(), op.getValue());
    }
}

void updateFinalRegState(stf::STFRegState& reg_state, const stf::STFInstReader::iterator& it) {
    for(const auto& op: it->getDestOperands()) {
        reg_state.regStateUpdate(op.getReg(), op.getValue());
    }
}

int main(int argc, char** argv) {
    try {
        const STFEditConfig config = parseCommandLine(argc, argv);
        stf::STFInstReader reader(config.trace);
        stf::STFWriter writer(config.output);
        reader.copyHeader(writer);
        writer.finalizeHeader();

        auto it = config.start_inst > 1 ? reader.seekFromBeginning(config.start_inst - 1) : reader.begin();
        const auto end_it = reader.end();

        stf::STFRegState reg_state(reader.getISA(), reader.getInitialIEM());

        for(; it != end_it; ++it) {
            const auto pc = it->pc();

            updateInitialRegState(reg_state, it);

            if(const auto morph_it = config.opcode_morphs.find(pc);
               STF_EXPECT_FALSE(morph_it != config.opcode_morphs.end())) {
                size_t orig_inst_bytes_seen = 0;
                auto opcode_it = morph_it->second.getOpcodes().begin();
                const auto opcode_end_it = morph_it->second.getOpcodes().end();
                const auto total_morph_size = morph_it->second.getTotalSize();
                size_t morph_bytes_written = 0;
                bool increment_instruction_size = true;

                while(opcode_it != opcode_end_it || (orig_inst_bytes_seen < total_morph_size && it != end_it)) {
                    stf_assert(!it->isTakenBranch(),
                               "Taken branches cannot be replaced. Instruction at PC "
                               << std::hex
                               << pc
                               << " is a taken branch.");

                    if(opcode_it != opcode_end_it) {
                        morph_bytes_written += opcode_it->write(writer, reg_state);
                        ++opcode_it;
                    }

                    // Only count an original instruction if this is the first time we've seen it
                    if(increment_instruction_size) {
                        orig_inst_bytes_seen += it->opcodeSize();
                    }

                    // Move the reader forward if we haven't seen enough bytes to account for the replacement opcodes
                    if(orig_inst_bytes_seen < morph_bytes_written ||
                       (orig_inst_bytes_seen == morph_bytes_written && opcode_it != opcode_end_it)) {
                        updateFinalRegState(reg_state, it);
                        ++it;
                        updateInitialRegState(reg_state, it);
                        increment_instruction_size = true;
                    }
                    else {
                        increment_instruction_size = false;
                    }
                }

                // Make sure all of the original and new instruction bytes are accounted for
                stf_assert(orig_inst_bytes_seen == total_morph_size,
                           "Attempted to replace "
                           << orig_inst_bytes_seen
                           << " bytes of instructions with "
                           << total_morph_size
                           << " bytes of instructions");
            }
            else {
                it->write(writer);
            }

            updateFinalRegState(reg_state, it);
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
