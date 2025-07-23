#pragma once

#include <bitset>
#include <cstdlib>
#include <mavis/DecoderTypes.h>
#include <mavis/extension_managers/RISCVExtensionManager.hpp>
#include <memory>
#include <ostream>
#include <string_view>

#include "mavis_helpers.hpp"
#include "stf_isa_defaults.hpp"
#include "stf_valid_value.hpp"
#include "stf_record_types.hpp"
#include "stf_env_var.hpp"
#include "stf_reader.hpp"
#include "filesystem.hpp"
#include "tools_util.hpp"

namespace stf {
    /**
     * \class STFDecoderBase
     * \brief Class used by STF tools to decode instructions
     */
    template<typename InstType, typename AnnotationType, bool AllowEnvOverride>
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

            using MavisType = ::Mavis<InstType, AnnotationType>;

        protected:
            inline static const std::string UNIMP_ = "c.unimp";

            // These default values mean that:
            // * start tracepoint = xor x0, x0, x0
            // * stop tracepoint = xor x0, x1, x1
            // * start markpoint = or x0, x0, x0
            // * stop markpoint = or x0, x1, x1
            inline static constexpr uint32_t DEFAULT_START_TRACEPOINT_OPCODE_ = 0x00004033;
            inline static constexpr uint32_t DEFAULT_STOP_TRACEPOINT_OPCODE_ = 0x0010c033;
            inline static constexpr uint32_t DEFAULT_START_MARKPOINT_OPCODE_ = 0x00006033;
            inline static constexpr uint32_t DEFAULT_STOP_MARKPOINT_OPCODE_ = 0x0010e033;

            const std::string mavis_path_;
            const std::string mavis_isa_spec_;
            const std::string isa_string_;
            const std::string elf_;
            using ExtMan = mavis::extension_manager::riscv::RISCVExtensionManager;
            const ExtMan ext_man_;
            mutable MavisType mavis_; /**< Mavis decoder */

            // Cached instruction UIDs allow us to identify ecall/mret/sret/uret without needing to check the mnemonic
            const mavis::InstructionUniqueID ecall_uid_;
            const mavis::InstructionUniqueID mret_uid_;
            const mavis::InstructionUniqueID sret_uid_;
            const mavis::InstructionUniqueID uret_uid_;

            // Tracepoint info. Can be changed by the user if they do not want to use the default values
            uint32_t start_tracepoint_opcode_ = DEFAULT_START_TRACEPOINT_OPCODE_;
            uint32_t stop_tracepoint_opcode_ = DEFAULT_STOP_TRACEPOINT_OPCODE_;

            // Markpoint info. Can be changed by the user if they do not want to use the default values
            uint32_t start_markpoint_opcode_ = DEFAULT_START_MARKPOINT_OPCODE_;
            uint32_t stop_markpoint_opcode_ = DEFAULT_STOP_MARKPOINT_OPCODE_;

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

            template<typename>
            struct bitset_size;

            template<size_t N>
            struct bitset_size<std::bitset<N>> {
                static constexpr size_t size = N;
            };

            static ExtMan constructExtMan_(const std::string& isa_string, const std::string& mavis_isa_spec, const std::string& mavis_path, const std::string& elf) {
                const auto& json_path = mavis_helpers::getMavisJSONPath(mavis_path);

                if(!elf.empty()) {
                    try {
                        return ExtMan::fromELF(elf, mavis_isa_spec, json_path);
                    }
                    catch(const mavis::extension_manager::ELFNotFoundException&) {
                    }
                    catch(const mavis::extension_manager::riscv::ISANotFoundInELFException&) {
                    }
                }

                return ExtMan::fromISA(isa_string, mavis_isa_spec, json_path);
            }

            static inline std::string getISAString_(const std::string& isa_string) {
                if constexpr(AllowEnvOverride) {
                    return stf::STFEnvVar("STF_DISASM_ISA", isa_string).get();
                }
                else {
                    return isa_string;
                }
            }

            STFDecoderBase(const std::string& mavis_path, const std::string& mavis_isa_spec, const std::string& isa_string, const std::string& elf) :
                mavis_path_(mavis_path),
                mavis_isa_spec_(mavis_isa_spec),
                isa_string_(isa_string),
                elf_(elf),
                ext_man_(constructExtMan_(isa_string_, mavis_isa_spec_, mavis_path_, elf_)),
                mavis_(ext_man_.constructMavis<InstType, AnnotationType>({})),
                ecall_uid_(lookupInstructionUID("ecall")),
                mret_uid_(lookupInstructionUID("mret")),
                sret_uid_(lookupInstructionUID("sret")),
                uret_uid_(lookupInstructionUID("uret"))
            {
            }

        public:
            /**
             * Constructs an STFDecoderBase
             * \param iem Instruction encoding
             */
            STFDecoderBase(const stf::ISA isa, const stf::INST_IEM iem) :
                STFDecoderBase(getDefaultPath_(), isa, iem)
            {
            }

            /**
             * Constructs an STFDecoderBase
             * \param iem Instruction encoding
             * \param isa_string RISC-V ISA string used to configure Mavis
             */
            STFDecoderBase(const stf::ISA isa, const stf::INST_IEM iem, const std::string& isa_string, const std::string& elf = "") :
                STFDecoderBase(getDefaultPath_(), isa, iem, isa_string)
            {
            }

            /**
             * Constructs an STFDecoderBase
             * \param mavis_path Path to Mavis checkout
             * \param iem Instruction encoding
             */
            STFDecoderBase(const std::string& mavis_path, const stf::ISA isa, const stf::INST_IEM iem) :
                STFDecoderBase(mavis_path, isa, iem, stf::ISADefaults::getISAExtendedInfo(isa, iem))
            {
            }

            /**
             * Constructs an STFDecoderBase
             * \param mavis_path Path to Mavis checkout
             * \param iem Instruction encoding
             * \param isa_string RISC-V ISA string used to configure Mavis
             */
            STFDecoderBase(const std::string& mavis_path, const stf::ISA isa, const stf::INST_IEM iem, const std::string& isa_string, const std::string& elf = "") :
                STFDecoderBase(mavis_path, mavis_helpers::getISASpecPath(mavis_path, isa, iem), getISAString_(isa_string), elf)
            {
            }

            STFDecoderBase(const std::string& mavis_path, const STFReader& reader, const std::string& elf = "") :
                STFDecoderBase(mavis_path, reader.getISA(), reader.getInitialIEM(), reader.getISAExtendedInfo(), elf)
            {
            }

            explicit STFDecoderBase(const STFReader& reader, const std::string& elf = "") :
                STFDecoderBase(getDefaultPath_(), reader, elf)
            {
            }

            /**
             * Copy constructor
             */
            STFDecoderBase(const STFDecoderBase& rhs) :
                mavis_path_(rhs.mavis_path_),
                mavis_isa_spec_(rhs.mavis_isa_spec_),
                isa_string_(rhs.isa_string_),
                elf_(rhs.elf_),
                ext_man_(constructExtMan_(isa_string_, mavis_isa_spec_, mavis_path_, elf_)),
                mavis_(ext_man_.constructMavis<InstType, AnnotationType>({})),
                ecall_uid_(lookupInstructionUID("ecall")),
                mret_uid_(lookupInstructionUID("mret")),
                sret_uid_(lookupInstructionUID("sret")),
                uret_uid_(lookupInstructionUID("uret")),
                start_tracepoint_opcode_(rhs.start_tracepoint_opcode_),
                stop_tracepoint_opcode_(rhs.stop_tracepoint_opcode_),
                start_markpoint_opcode_(rhs.start_markpoint_opcode_),
                stop_markpoint_opcode_(rhs.stop_markpoint_opcode_)
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
                return isStartMarkpoint() || isStopMarkpoint();
            }

            /**
             * Returns whether the current decoded instruction is a start markpoint
             */
            inline bool isStartMarkpoint() const {
                return opcode_.get() == start_markpoint_opcode_;
            }

            /**
             * Returns whether the current decoded instruction is a stop markpoint
             */
            inline bool isStopMarkpoint() const {
                return opcode_.get() == stop_markpoint_opcode_;
            }

            /**
             * Returns whether the current decoded instruction is a tracepoint
             */
            inline bool isTracepoint() const {
                return isStartTracepoint() || isStopTracepoint();
            }

            /**
             * Returns whether the current decoded instruction is a start tracepoint
             */
            inline bool isStartTracepoint() const {
                return opcode_.get() == start_tracepoint_opcode_;
            }

            /**
             * Returns whether the current decoded instruction is a stop tracepoint
             */
            inline bool isStopTracepoint() const {
                return opcode_.get() == stop_tracepoint_opcode_;
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
            void setStartMarkpointOpcode(const uint32_t opcode) {
                stf_assert(opcode != 0, "Invalid start markpoint opcode");
                start_markpoint_opcode_ = opcode;
            }

            uint32_t getStartMarkpointOpcode() const {
                return start_markpoint_opcode_;
            }

            void setStopMarkpointOpcode(const uint32_t opcode) {
                stf_assert(opcode != 0, "Invalid stop markpoint opcode");
                stop_markpoint_opcode_ = opcode;
            }

            uint32_t getStopMarkpointOpcode() const {
                return stop_markpoint_opcode_;
            }

            void setStartTracepointOpcode(const uint32_t opcode) {
                stf_assert(opcode != 0, "Invalid start tracepoint opcode");
                start_tracepoint_opcode_ = opcode;
            }

            uint32_t getStartTracepointOpcode() const {
                return start_tracepoint_opcode_;
            }

            void setStopTracepointOpcode(const uint32_t opcode) {
                stf_assert(opcode != 0, "Invalid stop tracepoint opcode");
                stop_tracepoint_opcode_ = opcode;
            }

            uint32_t getStopTracepointOpcode() const {
                return stop_tracepoint_opcode_;
            }
    };

    template<bool AllowEnvOverride>
    class STFDecoderImpl : public STFDecoderBase<mavis_helpers::InstType, mavis_helpers::DummyAnnotationType, AllowEnvOverride> {
        private:
            using ParentType = STFDecoderBase<mavis_helpers::InstType, mavis_helpers::DummyAnnotationType, AllowEnvOverride>;

        public:
            using ParentType::ParentType;

            template<typename T>
            STFDecoderImpl& decode(const T& op) {
                ParentType::decode_(op);
                return *this;
            }
    };

    template<bool AllowEnvOverride>
    class STFDecoderFullImpl : public STFDecoderBase<mavis_helpers::InstType, mavis_helpers::AnnotationType, AllowEnvOverride> {
        private:
            using ParentType = STFDecoderBase<mavis_helpers::InstType, mavis_helpers::AnnotationType, AllowEnvOverride>;

        public:
            using ParentType::ParentType;

            template<typename T>
            STFDecoderFullImpl& decode(const T& op) {
                ParentType::decode_(op);
                return *this;
            }

            const auto& getAnnotation() const {
                return ParentType::getDecodeInfo_()->uinfo;
            }
    };

    using STFDecoder = STFDecoderImpl<true>;
    using STFDecoderStrict = STFDecoderImpl<false>;
    using STFDecoderFull = STFDecoderFullImpl<true>;
} // end namespace stf
