#include "stf_morpher.hpp"

size_t STFMorpher::OpcodeMorph::Op::write(stf::STFWriter& writer, const stf::STFRegState& reg_state) const {
    for(auto& op: operands_) {
        if(STF_EXPECT_FALSE(op.isVector())) {
            stf::InstRegRecord::VectorType reg_data(stf::InstRegRecord::calcVectorLen(op.getVLen()));

            // Grab the latest values for each operand
            try {
                reg_data = reg_state.getRegVectorValue(op.getReg());
            }
            catch(const stf::STFRegState::RegNotFoundException&) {
                // Defaults to 0 if we haven't seen the register in the trace yet
            }

            op.setVectorData(reg_data);
        }
        else {
            uint64_t reg_data = 0;

            // Grab the latest values for each operand
            try {
                reg_data = reg_state.getRegScalarValue(op.getReg());
            }
            catch(const stf::STFRegState::RegNotFoundException&) {
                // Defaults to 0 if we haven't seen the register in the trace yet
            }

            op.setScalarData(reg_data);
        }
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

void STFMorpher::processOpcodeMorphArguments_(const trace_tools::CommandLineParser& parser) {
    for(auto morph_type = stf::enums::to_int(MorphType::STFID); morph_type < stf::enums::to_int(MorphType::NUM_TYPES); ++morph_type) {
        uint64_t global_address = 0;
        const bool has_global_address = parser.hasArgument('A');
        parser.getArgumentValue<uint64_t, 16>('A', global_address);

        uint16_t global_size = 0;
        const bool has_global_size = parser.hasArgument('S');
        parser.getArgumentValue('S', global_size);

        const char argument_flag = getArgumentFlag_(morph_type);

        auto& opcode_morphs = morphs_[morph_type];

        for(const auto& morph: parser.getMultipleValueArgument(argument_flag)) {
            const auto eq_pos = morph.find('=');
            stf_assert(eq_pos != std::string::npos, "Invalid -a value specified: " << morph);

            const auto id_str = morph.substr(0, eq_pos);
            const uint64_t id = convertMorphId_(morph_type, id_str);
            std::stringstream opcode_stream(morph.substr(eq_pos + 1));

            try {
                std::string opcode_str;
                const auto result = opcode_morphs.try_emplace(id);
                if(STF_EXPECT_FALSE(!result.second)) {
                    switch(static_cast<MorphType>(morph_type)) {
                        case MorphType::PC:
                            parser.raiseErrorWithHelp("PC " + id_str + " specified multiple times.");
                            break;
                        case MorphType::STFID:
                            parser.raiseErrorWithHelp("STFID " + id_str + " specified multiple times.");
                            break;
                        case MorphType::NUM_TYPES:
                            break;
                    }

                    stf_throw("Invalid morph type");
                }

                const auto morph_it = result.first;
                while(getline(opcode_stream, opcode_str, ',')) {
                    const auto at_pos = opcode_str.find('@');
                    const bool has_at = at_pos != std::string::npos;
                    uint32_t opcode = 0;
                    uint64_t address = 0;
                    uint16_t size = 0;
                    if(has_at) {
                        const auto colon_pos = opcode_str.find(':');
                        if(STF_EXPECT_FALSE(colon_pos == std::string::npos)) {
                            parser.assertCondition(has_global_size,
                                                   "Did not specify an access size for an LS op: ", opcode_str);
                            size = global_size;
                        }
                        address = std::stoull(opcode_str.substr(at_pos + 1, colon_pos - at_pos - 1), 0, 16);
                        size = static_cast<uint16_t>(std::stoul(opcode_str.substr(colon_pos + 1)));
                        opcode = static_cast<uint32_t>(std::stoul(opcode_str.substr(0, at_pos), 0, 16));
                    }
                    else {
                        opcode = static_cast<uint32_t>(std::stoul(opcode_str, 0, 16));
                    }

                    decoder_.decode(opcode);

                    parser.assertCondition(!decoder_.isBranch(),
                                           "Instructions cannot be replaced with branches. Opcode ",
                                           opcode_str,
                                           " is a branch.");

                    const bool ls_op = decoder_.isLoad() || decoder_.isStore();

                    if(has_at) {
                        parser.assertCondition(ls_op,
                                               "Specified a destination/source address for a non-LS op: ",
                                               opcode_str);
                    }
                    else if(ls_op) {
                        parser.assertCondition(has_global_address,
                                               "Attempted to add a load/store op without specifying the destination/source address: ",
                                               opcode_str);
                        address = global_address;
                    }

                    const size_t inst_size = decoder_.isCompressed() ? 2 : 4;
                    morph_it->second.addOp(opcode,
                                           decoder_.getRegisterOperands(),
                                           address,
                                           size,
                                           decoder_.getMemAccessType(),
                                           inst_size);
                }
            }
            catch(const stf::STFDecoder::InvalidInstException& e) {
                parser.raiseErrorWithHelp(std::string("Invalid opcode specified: ") + e.what());
            }
        }
    }
}

void STFMorpher::process() {
    reader_.copyHeader(writer_);
    writer_.addTraceInfo(stf::STF_GEN::STF_GEN_STF_MORPH,
                         TRACE_TOOLS_VERSION_MAJOR,
                         TRACE_TOOLS_VERSION_MINOR,
                         TRACE_TOOLS_VERSION_MINOR_MINOR,
                         "Trace morphed with stf_morph");
    writer_.finalizeHeader();

    const auto& end_it = reader_.end();
    for(; it_ != end_it; ++it_) {
        if(STF_EXPECT_FALSE(end_inst_ && it_->index() > end_inst_)) {
            break;
        }

        updateInitialRegState_();
        bool morph_found = false;

        for(auto type = stf::enums::to_int(MorphType::STFID); type < stf::enums::to_int(MorphType::NUM_TYPES); ++type) {
            const auto& morphs = morphs_[type];
            const auto index = getMorphIndex_(type);
            auto morph_it = morphs.find(index);
            const auto morph_end_it = morphs.cend();

            if(STF_EXPECT_TRUE(morph_it == morph_end_it)) {
                continue;
            }

            if(STF_EXPECT_FALSE(morph_found)) {
                stf_assert(allow_collisions_,
                           "Morph at index " << formatMorphIndex_(type, index) << " collides with another morph");
                break;
            }

            uint64_t pc = it_->pc();
            size_t orig_inst_bytes_seen = 0;
            auto opcode_it = morph_it->second.getOpcodes().begin();
            const auto opcode_end_it = morph_it->second.getOpcodes().end();
            const auto total_morph_size = morph_it->second.getTotalSize();
            size_t morph_bytes_written = 0;
            bool increment_instruction_size = true;

            writer_ << stf::CommentRecord("BEGIN MORPH");

            while(opcode_it != opcode_end_it || (orig_inst_bytes_seen < total_morph_size && it_ != end_it)) {
                // Only count an original instruction if this is the first time we've seen it
                if(increment_instruction_size) {
                    const auto orig_inst_size = it_->opcodeSize();

                    if(STF_EXPECT_FALSE(it_->isTakenBranch())) {
                        stf_assert(static_cast<MorphType>(type) == MorphType::PC,
                                   "Branches can only be replaced on a PC-basis.");

                        const uint64_t target = it_->branchTarget();

                        stf_assert(target > pc,
                                   "Backwards taken branches cannot be replaced. Instruction at PC "
                                   << std::hex
                                   << pc
                                   << " is a backwards taken branch.");

                        stf_assert(!decoder_.decode(it_->opcode()).isIndirect(),
                                   "Indirect branches cannot be replaced. Instruction at PC "
                                   << std::hex
                                   << pc
                                   << " is an indirect branch.");

                        orig_inst_bytes_seen += target - pc;
                    }
                    else {
                        orig_inst_bytes_seen += orig_inst_size;
                    }
                }

                if(opcode_it != opcode_end_it) {
                    morph_bytes_written += opcode_it->write(writer_, reg_state_);
                    ++opcode_it;
                }

                // Move the reader forward if we haven't seen enough bytes to account for the replacement opcodes
                if(orig_inst_bytes_seen < morph_bytes_written ||
                   (orig_inst_bytes_seen == morph_bytes_written && opcode_it != opcode_end_it)) {
                    updateFinalRegState_();
                    ++it_;
                    updateInitialRegState_();
                    pc = it_->pc();
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

            writer_ << stf::CommentRecord("END MORPH");

            morph_found = true;
        }

        if(STF_EXPECT_TRUE(!morph_found)) {
            // Write the original instruction if there were no morphs for it
            it_->write(writer_);
        }

        updateFinalRegState_();
    }
}
