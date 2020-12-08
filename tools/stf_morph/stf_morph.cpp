#include <string>
#include <unordered_map>
#include <vector>

#include "command_line_parser.hpp"

#include "stf_decoder.hpp"
#include "stf_inst_reader.hpp"
#include "stf_reg_state.hpp"
#include "stf_writer.hpp"
#include "stf_record_types.hpp"

struct OpcodeMorph {
    struct Op {
        enum class TYPE {
            REG,
            LOAD,
            STORE
        };

        uint32_t opcode;
        std::vector<stf::InstRegRecord> operands;
        uint64_t ls_address;
        uint16_t ls_size;
        TYPE type;
        bool compressed;

        Op(const uint32_t opcode,
           std::vector<stf::InstRegRecord>&& operands,
           const uint64_t ls_address,
           const uint16_t ls_size,
           const bool is_load,
           const bool is_store,
           const bool is_compressed) :
            opcode(opcode),
            operands(std::move(operands)),
            ls_address(ls_address),
            type(is_load ? TYPE::LOAD : (is_store ? TYPE::STORE : TYPE::REG)),
            compressed(is_compressed)
        {
        }

        bool isLoad() const {
            return type == TYPE::LOAD;
        }

        bool isStore() const {
            return type == TYPE::STORE;
        }
    };

    size_t opcode_size;
    std::vector<Op> opcodes;
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
    parser.addMultiFlag('a', "pc=opcode1[->addr1:size1][,opcode2[->addr2:size2],...]", "morph instruction at pc to specified opcode(s). LS instructions can have target addresses and access sizes specified with `opcode->addr:size` syntax");
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
                const auto arrow_pos = opcode_str.find("->");
                uint32_t opcode = 0;
                uint64_t address = 0;
                uint16_t size = 0;
                if(arrow_pos != std::string::npos) {
                    const auto colon_pos = opcode_str.find(':');
                    if(STF_EXPECT_FALSE(colon_pos == std::string::npos)) {
                        if(has_global_size) {
                            size = global_size;
                        }
                        else {
                            parser.raiseErrorWithHelp("Did not specify an access size for an LS op: " + opcode_str);
                        }
                    }
                    address = std::stoull(opcode_str.substr(arrow_pos + 2, colon_pos - arrow_pos - 2), 0, 16);
                    size = static_cast<uint16_t>(std::stoul(opcode_str.substr(colon_pos + 1)));
                    opcode = static_cast<uint32_t>(std::stoul(opcode_str.substr(0, arrow_pos), 0, 16));
                }
                else {
                    opcode = static_cast<uint32_t>(std::stoul(opcode_str, 0, 16));
                }
                decoder.decode(opcode);

                if(STF_EXPECT_FALSE(decoder.isBranch())) {
                    parser.raiseErrorWithHelp("Instructions cannot be replaced with branches. Opcode " + opcode_str + " is a branch.");
                }
                else if(STF_EXPECT_FALSE(arrow_pos != std::string::npos && !(decoder.isLoad() || decoder.isStore()))) {
                    parser.raiseErrorWithHelp("Specified a destination/source address for a non-LS op: " + opcode_str);
                }
                else if(STF_EXPECT_FALSE(arrow_pos == std::string::npos && (decoder.isLoad() || decoder.isStore()))) {
                    if(has_global_address) {
                        address = global_address;
                    }
                    else {
                        parser.raiseErrorWithHelp("Attempted to add a load/store op without specifying the destination/source address: " + opcode_str);
                    }
                }

                const bool is_compressed = decoder.isCompressed();
                morph_it->second.opcodes.emplace_back(opcode,
                                                      decoder.getRegisterOperands(),
                                                      address,
                                                      size,
                                                      decoder.isLoad(),
                                                      decoder.isStore(),
                                               is_compressed);

                morph_it->second.opcode_size += is_compressed ? 2 : 4;
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

int main(int argc, char** argv) {
    try {
        STFEditConfig config = parseCommandLine(argc, argv);
        stf::STFInstReader reader(config.trace);
        stf::STFWriter writer(config.output);
        reader.copyHeader(writer);
        writer.finalizeHeader();

        auto it = config.start_inst > 1 ? reader.seekFromBeginning(config.start_inst - 1) : reader.begin();
        const auto end_it = reader.end();

        stf::STFRegState reg_state(reader.getISA(), reader.getInitialIEM());

        for(; it != end_it; ++it) {
            const auto& inst = *it;
            const auto pc = inst.pc();

            for(const auto& op: inst.getRegisterStates()) {
                reg_state.regStateUpdate(op.getReg(), op.getValue());
            }

            for(const auto& op: inst.getSourceOperands()) {
                reg_state.regStateUpdate(op.getReg(), op.getValue());
            }

            if(const auto morph_it = config.opcode_morphs.find(pc);
               STF_EXPECT_FALSE(morph_it != config.opcode_morphs.end())) {
                stf_assert(inst.opcodeSize() == morph_it->second.opcode_size,
                           "Attempted to replace a " << inst.opcodeSize() << "-byte instruction with " << morph_it->second.opcode_size << " bytes of instructions");
                stf_assert(!inst.isTakenBranch(),
                           "Taken branches cannot be replaced. Instruction at PC " << std::hex << pc << " is a taken branch.");

                for(auto& opcode: morph_it->second.opcodes) {
                    for(auto& op: opcode.operands) {
                        uint64_t reg_data = 0;

                        try {
                            reg_state.getRegValue(op.getReg(), reg_data);
                        }
                        catch(const stf::STFRegState::RegNotFoundException&) {
                        }

                        op.setData(reg_data);
                        writer << op;
                    }

                    stf::INST_MEM_ACCESS access_type = stf::INST_MEM_ACCESS::INVALID;
                    if(STF_EXPECT_FALSE(opcode.isLoad())) {
                        access_type = stf::INST_MEM_ACCESS::READ;
                    }
                    else if(STF_EXPECT_FALSE(opcode.isStore())) {
                        access_type = stf::INST_MEM_ACCESS::WRITE;
                    }

                    if(STF_EXPECT_FALSE(access_type != stf::INST_MEM_ACCESS::INVALID)) {
                        writer << stf::InstMemAccessRecord(opcode.ls_address, opcode.ls_size, 0, access_type);
                        writer << stf::InstMemContentRecord(0);
                    }

                    if(opcode.compressed) {
                        writer << stf::InstOpcode16Record(static_cast<uint16_t>(opcode.opcode));
                    }
                    else {
                        writer << stf::InstOpcode32Record(opcode.opcode);
                    }
                }
            }
            else {
                inst.write(writer);
            }

            for(const auto& op: inst.getDestOperands()) {
                reg_state.regStateUpdate(op.getReg(), op.getValue());
            }
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
