#pragma once

#include <bitset>
#include <cstdlib>
#include <mavis/DecoderTypes.h>
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

            // These default values mean that:
            // * start tracepoint = xor x0, x0, x0
            // * stop tracepoint = xor x0, x1, x1
            // * start markpoint = or x0, x0, x0
            // * stop markpoint = or x0, x1, x1
            inline static constexpr std::string_view DEFAULT_MARKPOINT_MNEMONIC_ = "or";
            inline static constexpr std::string_view DEFAULT_TRACEPOINT_MNEMONIC_ = "xor";
            inline static constexpr mavis::OperandInfo::OpcodeFieldValueType DEFAULT_START_TRACEPOINT_VALUE_ = 0;
            inline static constexpr mavis::OperandInfo::OpcodeFieldValueType DEFAULT_STOP_TRACEPOINT_VALUE_ = 1;
            inline static constexpr mavis::OperandInfo::OpcodeFieldValueType DEFAULT_START_MARKPOINT_VALUE_ = 0;
            inline static constexpr mavis::OperandInfo::OpcodeFieldValueType DEFAULT_STOP_MARKPOINT_VALUE_ = 1;

            const std::vector<std::string> isa_jsons_; /**< Cached paths to ISA jsons so that we can make a copy of this decoder */
            const std::vector<std::string> anno_jsons_; /**< Cached paths to annotation jsons so that we can make a copy of this decoder */

            mutable MavisType mavis_; /**< Mavis decoder */

            // Cached instruction UIDs allow us to identify ecall/mret/sret/uret without needing to check the mnemonic
            const mavis::InstructionUniqueID ecall_uid_;
            const mavis::InstructionUniqueID mret_uid_;
            const mavis::InstructionUniqueID sret_uid_;
            const mavis::InstructionUniqueID uret_uid_;

            // Tracepoint info. Can be changed by the user if they do not want to use the default values
            std::string tracepoint_mnemonic_{DEFAULT_TRACEPOINT_MNEMONIC_};
            mavis::InstructionUniqueID tracepoint_uid_;
            mavis::OperandInfo::OpcodeFieldValueType start_tracepoint_value_ = DEFAULT_START_TRACEPOINT_VALUE_;
            mavis::OperandInfo::OpcodeFieldValueType stop_tracepoint_value_ = DEFAULT_STOP_TRACEPOINT_VALUE_;

            // Markpoint info. Can be changed by the user if they do not want to use the default values
            std::string markpoint_mnemonic_{DEFAULT_MARKPOINT_MNEMONIC_};
            mavis::InstructionUniqueID markpoint_uid_;
            mavis::OperandInfo::OpcodeFieldValueType start_markpoint_value_ = DEFAULT_START_MARKPOINT_VALUE_;
            mavis::OperandInfo::OpcodeFieldValueType stop_markpoint_value_ = DEFAULT_STOP_MARKPOINT_VALUE_;

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

                const auto exe_dir = getExecutablePath().parent_path();

                const auto relative_path = exe_dir / DEFAULT_RELATIVE_JSON_PATH_;

                if(fs::exists(relative_path)) {
                    return relative_path;
                }

                static constexpr std::string_view SHARE_RELATIVE_JSON_PATH_ = "../share/stf_tools/mavis";

                const auto share_relative_path = exe_dir / SHARE_RELATIVE_JSON_PATH_;

                if(fs::exists(share_relative_path)) {
                    return share_relative_path;
                }

                static constexpr std::string_view SHARE_GLOBAL_JSON_PATH_ = "/usr/share/stf_tools/mavis";

                if(fs::exists(SHARE_GLOBAL_JSON_PATH_)) {
                    return std::string(SHARE_GLOBAL_JSON_PATH_);
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
             * Decodes an instruction from an STFRecord
             * \param rec Record to decode
             */
            inline void decode_(const STFRecord& rec) {
                if(rec.getId() == stf::descriptors::internal::Descriptor::STF_INST_OPCODE16) {
                    decode_(rec.as<InstOpcode16Record>());
                }
                else if(rec.getId() == stf::descriptors::internal::Descriptor::STF_INST_OPCODE32) {
                    decode_(rec.as<InstOpcode32Record>());
                }
                else {
                    stf_throw("Attempted to decode a non-instruction: " << rec.getId());
                }
            }

            /**
             * Decodes an instruction from an InstOpcode16Record
             * \param rec Record to decode
             */
            inline void decode_(const InstOpcode16Record& rec) {
                decode_(rec.getOpcode());
            }

            /**
             * Decodes an instruction from an InstOpcode32Record
             * \param rec Record to decode
             */
            inline void decode_(const InstOpcode32Record& rec) {
                decode_(rec.getOpcode());
            }

            /**
             * Decodes an instruction from a raw opcode
             * \param opcode Opcode to decode
             */
            inline void decode_(const uint32_t opcode) {
                is_compressed_ = isCompressed(opcode);
                if(!opcode_.valid() || opcode_.get() != opcode) {
                    opcode_ = opcode;
                    has_pending_decode_info_ = true;
                }
            }

            /**
             * Checks if a the current decoded instruction matches a tracepoint/markpoint form, i.e.:
             * It matches the tracepoint/markpoint mavis UID
             * Destination register is X0
             * Both source registers are the same
             */
            inline bool isTraceOrMarkpoint_(const mavis::InstructionUniqueID uid) const {
                if(getInstructionUID() != uid) {
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

                return true;
            }

            /**
             * Checks if a the current decoded instruction matches is a specific tracepoint/markpoint form, i.e.:
             * It matches the tracepoint/markpoint mavis UID
             * Destination register is X0
             * Both source registers are the expected value for the tracepoint/markpoint
             */
            inline bool isSpecificTraceOrMarkpoint_(const mavis::InstructionUniqueID uid,
                                                    const mavis::OperandInfo::OpcodeFieldValueType value) const {
                if(getInstructionUID() != uid) {
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
                    if(op_list[0].field_value != value || op_list[1].field_value != value) {
                        return false;
                    }
                }
                catch(const InvalidInstException&) {
                    return false;
                }

                return true;
            }

            template<typename>
            struct bitset_size;

            template<size_t N>
            struct bitset_size<std::bitset<N>> {
                static constexpr size_t size = N;
            };

            STFDecoderBase(std::vector<std::string>&& isa_jsons,
                           std::vector<std::string>&& anno_jsons) :
                isa_jsons_(std::move(isa_jsons)),
                anno_jsons_(std::move(anno_jsons)),
                mavis_(isa_jsons_, anno_jsons_),
                ecall_uid_(lookupInstructionUID("ecall")),
                mret_uid_(lookupInstructionUID("mret")),
                sret_uid_(lookupInstructionUID("sret")),
                uret_uid_(lookupInstructionUID("uret")),
                tracepoint_uid_(lookupInstructionUID(tracepoint_mnemonic_)),
                markpoint_uid_(lookupInstructionUID(markpoint_mnemonic_))
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
                STFDecoderBase(mavis_path, mavis_helpers::getMavisJSONs(mavis_path, iem), {})
            {
            }

            /**
             * Copy constructor
             */
            STFDecoderBase(const STFDecoderBase& rhs) :
                isa_jsons_(rhs.isa_jsons_),
                anno_jsons_(rhs.anno_jsons_),
                mavis_(isa_jsons_, anno_jsons_),
                ecall_uid_(lookupInstructionUID("ecall")),
                mret_uid_(lookupInstructionUID("mret")),
                sret_uid_(lookupInstructionUID("sret")),
                uret_uid_(lookupInstructionUID("uret")),
                tracepoint_mnemonic_(rhs.tracepoint_mnemonic_),
                tracepoint_uid_(lookupInstructionUID(tracepoint_mnemonic_)),
                start_tracepoint_value_(rhs.start_tracepoint_value_),
                stop_tracepoint_value_(rhs.stop_tracepoint_value_),
                markpoint_mnemonic_(rhs.markpoint_mnemonic_),
                markpoint_uid_(lookupInstructionUID(markpoint_mnemonic_)),
                start_markpoint_value_(rhs.start_markpoint_value_),
                stop_markpoint_value_(rhs.stop_markpoint_value_)
            {
                if(rhs.opcode_.valid()) {
                    decode_(rhs.opcode_.get());
                }
            }

            STFDecoderBase(STFDecoderBase&&) = default;
            STFDecoderBase& operator=(STFDecoderBase&&) = default;

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
                return isInstType(mavis::InstMetaData::InstructionTypes::JALR);
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
             * Returns whether the decoded instruction is a integer divide instruction
             */
            inline bool isDivide() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::DIVIDE);
            }

            /**
             * Returns whether the decoded instruction is a integer multiply instruction
             */
            inline bool isMultiply() const {
                return isInstType(mavis::InstMetaData::InstructionTypes::MULTIPLY);
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

                const auto uid = getInstructionUID();
                return uid == mret_uid_ || uid == sret_uid_ || uid == uret_uid_;
            }

            /**
             * Returns whether the decoded instruction is an exception return
             */
            inline bool isSyscall() const {
                if(!isInstType(mavis::InstMetaData::InstructionTypes::SYSTEM)) {
                    return false;
                }

                return getInstructionUID() == ecall_uid_;
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

            /**
             * Returns whether the current decoded instruction is a markpoint
             */
            inline bool isMarkpoint() const {
                return isTraceOrMarkpoint_(markpoint_uid_);
            }

            /**
             * Returns whether the current decoded instruction is a start markpoint
             */
            inline bool isStartMarkpoint() const {
                return isSpecificTraceOrMarkpoint_(markpoint_uid_, start_markpoint_value_);
            }

            /**
             * Returns whether the current decoded instruction is a stop markpoint
             */
            inline bool isStopMarkpoint() const {
                return isSpecificTraceOrMarkpoint_(markpoint_uid_, stop_markpoint_value_);
            }

            /**
             * Returns whether the current decoded instruction is a tracepoint
             */
            inline bool isTracepoint() const {
                return isTraceOrMarkpoint_(tracepoint_uid_);
            }

            /**
             * Returns whether the current decoded instruction is a start tracepoint
             */
            inline bool isStartTracepoint() const {
                return isSpecificTraceOrMarkpoint_(tracepoint_uid_, start_tracepoint_value_);
            }

            /**
             * Returns whether the current decoded instruction is a stop tracepoint
             */
            inline bool isStopTracepoint() const {
                return isSpecificTraceOrMarkpoint_(tracepoint_uid_, stop_tracepoint_value_);
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

            bool hasTag(const std::string& tag) const {
                return getDecodeInfo_()->opinfo->getTags().isMember(tag);
            }

            mavis::InstructionUniqueID lookupInstructionUID(const std::string& mnemonic) const {
                return mavis_.lookupInstructionUniqueID(mnemonic);
            }

            mavis::InstructionUniqueID getInstructionUID() const {
                return getDecodeInfo_()->opinfo->getInstructionUniqueID();
            }

            const std::string& getMnemonicFromUID(const mavis::InstructionUniqueID uid) const {
                return mavis_.lookupInstructionMnemonic(uid);
            }

            /**
             * Allows user to alter which instruction mnemonic is used for markpoints
             */
            void setMarkpointMnemonic(const std::string& mnemonic) {
                markpoint_mnemonic_ = mnemonic;
                markpoint_uid_ = lookupInstructionUID(mnemonic);
            }

            /**
             * Allows user to alter which register value indicates a start markpoint
             * Example: calling this method with value=4 would cause or x0, x4, x4 to be treated as
             * a start markpoint
             */
            void setStartMarkpointValue(const mavis::OperandInfo::OpcodeFieldValueType value) {
                start_markpoint_value_ = value;
            }

            /**
             * Allows user to alter which register value indicates a stop markpoint
             * Example: calling this method with value=5 would cause or x0, x5, x5 to be treated as
             * a stop markpoint
             */
            void setStopMarkpointValue(const mavis::OperandInfo::OpcodeFieldValueType value) {
                stop_markpoint_value_ = value;
            }

            /**
             * Allows user to alter which instruction mnemonic is used for tracepoints
             */
            void setTracepointMnemonic(const std::string& mnemonic) {
                tracepoint_mnemonic_ = mnemonic;
                tracepoint_uid_ = lookupInstructionUID(mnemonic);
            }

            /**
             * Allows user to alter which register value indicates a start tracepoint
             * Example: calling this method with value=4 would cause xor x0, x4, x4 to be treated as
             * a start tracepoint
             */
            void setStartTracepointValue(const mavis::OperandInfo::OpcodeFieldValueType value) {
                start_tracepoint_value_ = value;
            }

            /**
             * Allows user to alter which register value indicates a stop tracepoint
             * Example: calling this method with value=5 would cause xor x0, x5, x5 to be treated as
             * a stop tracepoint
             */
            void setStopTracepointValue(const mavis::OperandInfo::OpcodeFieldValueType value) {
                stop_tracepoint_value_ = value;
            }
    };

    class STFDecoder : public STFDecoderBase<mavis_helpers::Mavis> {
        public:
            explicit STFDecoder(const stf::INST_IEM iem) :
                STFDecoder(iem, getDefaultPath_())
            {
            }

            /**
             * Constructs an STFDecoderBase
             * \param iem Instruction encoding
             * \param mavis_path Path to Mavis checkout
             */
            STFDecoder(const stf::INST_IEM iem, const std::string& mavis_path) :
                STFDecoderBase(mavis_helpers::getMavisJSONs(mavis_path, iem), mavis_helpers::getMavisJSONs(mavis_path, iem))
            {
            }

            template<typename T>
            STFDecoder& decode(const T& op) {
                decode_(op);
                return *this;
            }
    };

    class STFDecoderFull : public STFDecoderBase<mavis_helpers::FullMavis> {
        public:
            explicit STFDecoderFull(const stf::INST_IEM iem) :
                STFDecoderFull(iem, getDefaultPath_())
            {
            }

            /**
             * Constructs an STFDecoderFull
             * \param iem Instruction encoding
             * \param mavis_path Path to Mavis checkout
             */
            STFDecoderFull(const stf::INST_IEM iem, const std::string& mavis_path) :
                STFDecoderBase(mavis_helpers::getMavisJSONs(mavis_path, iem), mavis_helpers::getMavisJSONs(mavis_path, iem))
            {
            }

            template<typename T>
            STFDecoderFull& decode(const T& op) {
                decode_(op);
                return *this;
            }

            const auto& getAnnotation() const {
                return getDecodeInfo_()->uinfo;
            }
    };

} // end namespace stf
