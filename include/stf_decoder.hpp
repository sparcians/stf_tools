#pragma once

#include <bitset>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <string_view>

#include "mavis_helpers.hpp"
#include "stf_valid_value.hpp"
#include "stf_record_types.hpp"
#include "filesystem.hpp"
#include "tools_util.hpp"

namespace stf {
    /**
     * \class STFDecoderBase
     * \brief Class used by STF tools to decode instructions
     */
    template<typename MavisType>
    class STFDecoderBase {
        public:
            /**
             * \class InvalidInstException
             * \brief Exception type thrown when the STFDecoder attempts to decode an invalid instruction
             */
            class InvalidInstException : public std::exception {
                private:
                    uint32_t opcode_;

                public:
                    InvalidInstException(const uint32_t opcode) :
                        opcode_(opcode)
                    {
                    }

                    const char* what() const noexcept final {
                        static thread_local std::string opcode_hex;
                        if(opcode_hex.empty()) {
                            std::stringstream ss;
                            ss << std::hex << opcode_;
                            opcode_hex = ss.str();
                        }
                        return opcode_hex.c_str();
                    }

                    uint32_t getOpcode() const {
                        return opcode_;
                    }
            };

        protected:
            inline static const std::string UNIMP_ = "c.unimp";

            mutable MavisType mavis_; /**< Mavis decoder */
            mutable typename MavisType::DecodeInfoType decode_info_; /**< Cached decode info */
            mutable bool has_pending_decode_info_ = false; /**< Cached decode info */
            stf::ValidValue<uint32_t> opcode_;
            bool is_compressed_ = false; /**< Set to true if the decoded instruction was compressed */
            mutable bool is_invalid_ = false;
            mutable std::string disasm_;
            mutable bool unknown_disasm_ = false;

            const typename MavisType::DecodeInfoType& getDecodeInfo_() const {
                if(STF_EXPECT_FALSE(has_pending_decode_info_)) {
                    try {
                        decode_info_ = mavis_.getInfo(opcode_.get());
                        has_pending_decode_info_ = false;
                        is_invalid_ = false;
                        disasm_.clear();
                    }
                    catch(const mavis::UnknownOpcode&) {
                        is_invalid_ = true;
                        throw InvalidInstException(opcode_.get());
                    }
                    catch(const mavis::IllegalOpcode&) {
                        is_invalid_ = true;
                        throw InvalidInstException(opcode_.get());
                    }
                }
                else if(STF_EXPECT_FALSE(is_invalid_)) {
                    throw InvalidInstException(opcode_.get());
                }

                stf_assert(decode_info_, "Attempted to get instruction info without calling decode() first.");
                return decode_info_;
            }

            /**
             * Gets a default path to Mavis
             */
            static std::string getDefaultPath_() {
                static constexpr std::string_view DEFAULT_RELATIVE_JSON_PATH_ = "../../../mavis"; /**< Default path to Mavis JSON files if building locally*/

                const auto mavis_path_env = getenv("MAVIS_PATH");
                if(mavis_path_env && fs::exists(mavis_path_env)) {
                    return std::string(mavis_path_env);
                }

                const auto relative_path = getExecutablePath().parent_path() / DEFAULT_RELATIVE_JSON_PATH_;

                if(fs::exists(relative_path)) {
                    return relative_path;
                }

#ifdef MAVIS_GLOBAL_PATH
                static constexpr std::string_view DEFAULT_GLOBAL_JSON_PATH_ = MAVIS_GLOBAL_PATH; /**< Default path to globally installed Mavis JSON files*/

                if(fs::exists(DEFAULT_GLOBAL_JSON_PATH_)) {
                    return std::string(DEFAULT_GLOBAL_JSON_PATH_);
                }
#endif

                stf_throw("Could not find Mavis JSON files anywhere. Please define the MAVIS_PATH environment variable to point to your Mavis checkout.");
            }

            /**
             * Returns true if the characters match
             * \param test character to test against
             * \param candidate candidate to test
             */
            inline static constexpr bool charMatches_(const char test, const char candidate) {
                return test == candidate;
            }

            /**
             * Returns true if the test character matches one of the candidates
             * \param test character to test against
             * \param candidate candidate to test
             * \param candidates additional candidates to test
             */
            template<typename ... CharArgs>
            inline static constexpr bool charMatches_(const char test, const char candidate, const CharArgs... candidates) {
                return charMatches_(test, candidate) || charMatches_(test, candidates...);
            }

            /**
             * Returns true if the instruction mnemonic is mode-prefixed.
             * e.g ecall, scall, mret, etc.
             * \param mnemonic mnemonic to check
             * \param suffix instruction suffix to check (e.g. call, ret, etc.)
             */
            inline static constexpr bool isModePrefixInstruction_(const std::string_view mnemonic,
                                                                  const std::string_view suffix) {
                if(STF_EXPECT_TRUE(mnemonic.size() != suffix.size() + 1)) {
                    return false;
                }
                if(STF_EXPECT_TRUE(!charMatches_(mnemonic.front(), 'e', 's', 'h', 'm'))) {
                    return false;
                }

                return mnemonic.compare(1, mnemonic.size() - 1, suffix) == 0;
            }

            template<typename>
            struct bitset_size;

            template<size_t N>
            struct bitset_size<std::bitset<N>> {
                static constexpr size_t size = N;
            };

            STFDecoderBase(std::vector<std::string>&& isa_jsons, std::vector<std::string>&& anno_jsons) :
                mavis_(std::move(isa_jsons), std::move(anno_jsons))
            {
            }

        public:
            /**
             * Constructs an STFDecoderBase
             * \param iem Instruction encoding
             */
            explicit STFDecoderBase(const stf::INST_IEM iem) :
                STFDecoderBase(iem, getDefaultPath_())
            {
            }

            /**
             * Constructs an STFDecoderBase
             * \param iem Instruction encoding
             * \param mavis_path Path to Mavis checkout
             */
            STFDecoderBase(const stf::INST_IEM iem, const std::string& mavis_path) :
                STFDecoderBase(mavis_helpers::getMavisJSONs(mavis_path, iem), {})
            {
            }

            STFDecoderBase(STFDecoderBase&&) = default;
            STFDecoderBase& operator=(STFDecoderBase&&) = default;

            /**
             * Decodes an instruction from an STFRecord
             * \param rec Record to decode
             */
            inline STFDecoderBase& decode(const STFRecord& rec) {
                if(rec.getId() == stf::descriptors::internal::Descriptor::STF_INST_OPCODE16) {
                    return decode(rec.as<InstOpcode16Record>());
                }
                else if(rec.getId() == stf::descriptors::internal::Descriptor::STF_INST_OPCODE32) {
                    return decode(rec.as<InstOpcode32Record>());
                }
                else {
                    stf_throw("Attempted to decode a non-instruction: " << rec.getId());
                }
            }

            /**
             * Decodes an instruction from an InstOpcode16Record
             * \param rec Record to decode
             */
            inline STFDecoderBase& decode(const InstOpcode16Record& rec) {
                return decode(rec.getOpcode());
            }

            /**
             * Decodes an instruction from an InstOpcode32Record
             * \param rec Record to decode
             */
            inline STFDecoderBase& decode(const InstOpcode32Record& rec) {
                return decode(rec.getOpcode());
            }

            /**
             * Decodes an instruction from a raw opcode
             * \param opcode Opcode to decode
             */
            inline STFDecoderBase& decode(uint32_t opcode) {
                is_compressed_ = isCompressed(opcode);
                if(!opcode_.valid() || opcode_.get() != opcode) {
                    opcode_ = opcode;
                    has_pending_decode_info_ = true;
                }
                return *this;
            }

            /**
             * Returns whether the decoded instruction has the given type
             * \param type Instruction type to check
             */
            inline bool isInstType(const mavis::InstMetaData::InstructionTypes type) const {
                try {
                    return getDecodeInfo_()->opinfo->isInstType(type);
                }
                catch(const InvalidInstException&) {
                    return false;
                }
            }

            /**
             * Returns all of the instruction types for the decoded instruction
             */
            inline auto getInstTypes() const {
                try {
                    return getDecodeInfo_()->opinfo->getInstType();
                }
                catch(const InvalidInstException&) {
                    return mavis_helpers::MavisInstTypeArray::int_t(0);
                }
            }

            /**
             * Returns whether the decoded instruction is a load
             */
            inline bool isLoad() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::LOAD);
            }

            /**
             * Returns whether the decoded instruction is a store
             */
            inline bool isStore() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::STORE);
            }

            /**
             * Returns whether the decoded instruction is atomic
             */
            inline bool isAtomic() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::ATOMIC);
            }

            /**
             * Returns the stf::INST_MEM_ACCESS type associated with the instruction
             */
            inline stf::INST_MEM_ACCESS getMemAccessType() const {
                const bool is_load = isLoad();
                const bool is_store = isStore();

                stf_assert(!(is_load && is_store), "Instruction cannot be both a load and a store");

                if(STF_EXPECT_FALSE(is_load)) {
                    return stf::INST_MEM_ACCESS::READ;
                }

                if(STF_EXPECT_FALSE(is_store)) {
                    return stf::INST_MEM_ACCESS::WRITE;
                }

                return stf::INST_MEM_ACCESS::INVALID;
            }

            /**
             * Returns whether the decoded instruction is a branch
             */
            inline bool isBranch() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::BRANCH);
            }

            /**
             * Returns whether the decoded instruction is conditional
             */
            inline bool isConditional() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::CONDITIONAL);
            }

            /**
             * Returns whether the decoded instruction is an indirect branch
             */
            inline bool isIndirect() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::JAL) ||
                       isInstType(mavis::InstMetaData::InstructionTypes::JALR);
            }

            /**
             * Returns whether the decoded instruction is a JALR instruction
             */
            inline bool isJal() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::JAL);
            }

            /**
             * Returns whether the decoded instruction is a JALR instruction
             */
            inline bool isJalr() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::JALR);
            }

            /**
             * Returns whether the decoded instruction is a AUIPC instruction
             */
            inline bool isAuipc() const {
                return (opcode_.get() & 0x0000007f) == 0x00000017;
            }

            /**
             * Returns whether the decoded instruction is a LUI instruction
             */
            inline bool isLui() const {
                return (opcode_.get() & 0x0000007f) == 0x00000037;
            }

            /**
             * Returns whether the decoded instruction is an exception return
             */
            inline bool isExceptionReturn() const {
                if(!isInstType(mavis::InstMetaData::InstructionTypes::SYSTEM)) {
                    return false;
                }

                return isModePrefixInstruction_(getMnemonic(), "ret");
            }

            /**
             * Returns whether the decoded instruction is an exception return
             */
            inline bool isSyscall() const {
                if(!isInstType(mavis::InstMetaData::InstructionTypes::SYSTEM)) {
                    return false;
                }

                return isModePrefixInstruction_(getMnemonic(), "call");
            }

            /**
             * Gets the mnemonic for the decoded instruction
             */
            inline const std::string& getMnemonic() const {
                try {
                    return getDecodeInfo_()->opinfo->getMnemonic();
                }
                catch(const InvalidInstException&) {
                    return UNIMP_;
                }
            }

            /**
             * Gets the disassembly for the decoded instruction
             */
            inline const std::string& getDisassembly() const {
                try {
                    const auto& decode_info = getDecodeInfo_();
                    if(STF_EXPECT_FALSE(disasm_.empty())) {
                        disasm_ = decode_info->opinfo->dasmString();
                    }
                }
                catch(const InvalidInstException& e) {
                    if(e.getOpcode() != 0) { // opcode == 0 usually implies a fault/interrupt in the trace
                        unknown_disasm_ = true;
                    }
                    return UNIMP_;
                }

                return disasm_;
            }

            /**
             * Gets the immediate for the decoded instruction
             */
            inline uint64_t getImmediate() const {
                try {
                    return getDecodeInfo_()->opinfo->getImmediate();
                }
                catch(const InvalidInstException&) {
                    return 0;
                }
            }

            /**
             * Gets whether the instruction has an immediate
             */
            inline bool hasImmediate() const {
                try {
                    return getDecodeInfo_()->opinfo->hasImmediate();
                }
                catch(const InvalidInstException&) {
                    return false;
                }
            }

            /**
             * Gets the sign-extended immediate for the decoded instruction
             */
            inline int64_t getSignedImmediate() const {
                try {
                    return getDecodeInfo_()->opinfo->getSignedOffset();
                }
                catch(const InvalidInstException&) {
                    return 0;
                }
            }

            /**
             * Gets a source register field for the decoded instruction
             */
            inline uint32_t getSourceRegister(const mavis::InstMetaData::OperandFieldID& fid) const {
                try {
                    return getDecodeInfo_()->opinfo->getSourceOpInfo().getFieldValue(fid);
                }
                catch(const InvalidInstException&) {
                    return 0;
                }
            }

            /**
             * Gets a destination register field for the decoded instruction
             */
            inline uint32_t getDestRegister(const mavis::InstMetaData::OperandFieldID& fid) const {
                try {
                    return getDecodeInfo_()->opinfo->getDestOpInfo().getFieldValue(fid);
                }
                catch(const InvalidInstException&) {
                    return 0;
                }
            }

            /**
             * Gets whether the opcode is compressed
             */
            inline static bool isCompressed(const uint32_t opcode) {
                return (opcode & 0x3) != 0x3;
            }

            /**
             * Gets whether the last decoded instruction was compressed
             */
            inline bool isCompressed() const {
                return is_compressed_;
            }

            std::vector<stf::InstRegRecord> getRegisterOperands() const {
                std::vector<stf::InstRegRecord> operands;
                const auto& op_info = getDecodeInfo_()->opinfo;

                const auto int_sources = op_info->getIntSourceRegs();
                const auto float_sources = op_info->getFloatSourceRegs();
                const auto vector_sources = op_info->getVectorSourceRegs();

                const auto int_dests = op_info->getIntDestRegs();
                const auto float_dests = op_info->getFloatDestRegs();
                const auto vector_dests = op_info->getVectorDestRegs();

                for(size_t i = 0; i < bitset_size<mavis::DecodedInstructionInfo::BitMask>::size; ++i) {
                    if(STF_EXPECT_FALSE(int_sources.test(i))) {
                        operands.emplace_back(i,
                                              stf::Registers::STF_REG_TYPE::INTEGER,
                                              stf::Registers::STF_REG_OPERAND_TYPE::REG_SOURCE,
                                              0);
                    }
                    if(STF_EXPECT_FALSE(float_sources.test(i))) {
                        operands.emplace_back(i,
                                              stf::Registers::STF_REG_TYPE::FLOATING_POINT,
                                              stf::Registers::STF_REG_OPERAND_TYPE::REG_SOURCE,
                                              0);
                    }
                    if(STF_EXPECT_FALSE(vector_sources.test(i))) {
                        operands.emplace_back(i,
                                              stf::Registers::STF_REG_TYPE::VECTOR,
                                              stf::Registers::STF_REG_OPERAND_TYPE::REG_SOURCE,
                                              0);
                    }
                    if(STF_EXPECT_FALSE(int_dests.test(i))) {
                        operands.emplace_back(i,
                                              stf::Registers::STF_REG_TYPE::INTEGER,
                                              stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                                              0);
                    }
                    if(STF_EXPECT_FALSE(float_dests.test(i))) {
                        operands.emplace_back(i,
                                              stf::Registers::STF_REG_TYPE::FLOATING_POINT,
                                              stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                                              0);
                    }
                    if(STF_EXPECT_FALSE(vector_dests.test(i))) {
                        operands.emplace_back(i,
                                              stf::Registers::STF_REG_TYPE::VECTOR,
                                              stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST,
                                              0);
                    }
                }

                return operands;
            }

            inline bool hasSourceRegister(const stf::Registers::STF_REG reg) const {
                const auto reg_num = stf::Registers::getArchRegIndex(reg);
                stf_assert(!stf::Registers::isCSR(reg), "CSRs are not supported yet");

                try {
                    if(stf::Registers::isFPR(reg)) {
                        return getDecodeInfo_()->opinfo->getFloatSourceRegs().test(reg_num);
                    }

                    return getDecodeInfo_()->opinfo->getIntSourceRegs().test(reg_num);
                }
                catch(const InvalidInstException&) {
                    return false;
                }
            }

            inline bool hasDestRegister(const stf::Registers::STF_REG reg) const {
                const auto reg_num = stf::Registers::getArchRegIndex(reg);
                stf_assert(!stf::Registers::isCSR(reg), "CSRs are not supported yet");

                try {
                    if(stf::Registers::isFPR(reg)) {
                        return getDecodeInfo_()->opinfo->getFloatDestRegs().test(reg_num);
                    }

                    return getDecodeInfo_()->opinfo->getIntDestRegs().test(reg_num);
                }
                catch(const InvalidInstException&) {
                    return false;
                }
            }

            inline bool isMarkpoint() const {
                if(!(isInstType(mavis::InstMetaData::InstructionTypes::INT) && isInstType(mavis::InstMetaData::InstructionTypes::ARITH))) {
                    return false;
                }

                if(!hasDestRegister(stf::Registers::STF_REG::STF_REG_X0)) {
                    return false;
                }

                try {
                    const auto& opinfo = getDecodeInfo_()->opinfo;
                    if(opinfo->numSourceRegs() != 2) {
                        return false;
                    }

                    const auto& op_list = opinfo->getSourceOpInfoList();
                    if(op_list[0].field_value != op_list[1].field_value) {
                        return false;
                    }
                }
                catch(const InvalidInstException&) {
                    return false;
                }

                return getMnemonic() == "or";
            }

            inline bool isTracepoint() const {
                if(!(isInstType(mavis::InstMetaData::InstructionTypes::INT) &&
                     isInstType(mavis::InstMetaData::InstructionTypes::ARITH))) {
                    return false;
                }

                if(!hasDestRegister(stf::Registers::STF_REG::STF_REG_X0)) {
                    return false;
                }

                try {
                    const auto& opinfo = getDecodeInfo_()->opinfo;
                    if(opinfo->numSourceRegs() != 2) {
                        return false;
                    }

                    const auto& op_list = opinfo->getSourceOpInfoList();
                    if(op_list[0].field_value != op_list[1].field_value) {
                        return false;
                    }
                }
                catch(const InvalidInstException&) {
                    return false;
                }

                return getMnemonic() == "xor";
            }

            /**
             * Returns whether an unknown instruction was ever encountered during disassembly
             */
            bool hasUnknownDisasm() const {
                return unknown_disasm_;
            }

            /**
             * Returns whether the last decode operation failed
             */
            bool decodeFailed() const {
                try {
                    getDecodeInfo_();
                    return false;
                }
                catch(const InvalidInstException&) {
                    return true;
                }
            }

            /**
             * Returns whether the instruction is from the bitmanip ISA extension
             */
            bool isBitmanip() const {
                try {
                    return getDecodeInfo_()->opinfo->isISA(mavis::OpcodeInfo::ISAExtension::B);
                }
                catch(const InvalidInstException&) {
                    return false;
                }
            }

            /**
             * Returns all of the ISA extensions an instruction belongs to
             */
            inline auto getISAExtensions() const {
                try {
                    return getDecodeInfo_()->opinfo->getISA();
                }
                catch(const InvalidInstException&) {
                    return mavis_helpers::MavisISAExtensionTypeArray::int_t(0);
                }
            }

            const auto& getAnnotation() const {
                return getDecodeInfo_()->uinfo;
            }
    };

    using STFDecoder = STFDecoderBase<mavis_helpers::Mavis>;

    class STFDecoderFull : public STFDecoderBase<mavis_helpers::FullMavis> {
        public:
            explicit STFDecoderFull(const stf::INST_IEM iem) :
                STFDecoderFull(iem, getDefaultPath_())
            {
            }

            /**
             * Constructs an STFDecoderBase
             * \param iem Instruction encoding
             * \param mavis_path Path to Mavis checkout
             */
            STFDecoderFull(const stf::INST_IEM iem, const std::string& mavis_path) :
                STFDecoderBase(mavis_helpers::getMavisJSONs(mavis_path, iem), mavis_helpers::getMavisJSONs(mavis_path, iem))
            {
            }

    };

} // end namespace stf
