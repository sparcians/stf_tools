#include <array>
#include <iostream>
#include <map>
#include <random>
#include <string_view>
#include <unordered_map>
#include <H5Cpp.h>

#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"

enum class HDF5Field {
    INDEX,
    PC,
    TARGET,
    OPCODE,
    TARGET_OPCODE,
    RS1,
    RS2,
    RS1_VALUE,
    RS2_VALUE,
    TAKEN,
    COND,
    CALL,
    RET,
    INDIRECT,
    COMPARE_EQ,
    COMPARE_NOT_EQ,
    COMPARE_GREATER_THAN_OR_EQUAL,
    COMPARE_LESS_THAN,
    COMPARE_UNSIGNED,
    LAST_TAKEN,
    WKLD_ID
};

#define PARSER_ENTRY(x) { #x, HDF5Field::x }

HDF5Field parseHDF5Field(const std::string_view hdf5_field_str) {
    static const std::unordered_map<std::string_view, HDF5Field> string_map {
        PARSER_ENTRY(INDEX),
        PARSER_ENTRY(PC),
        PARSER_ENTRY(TARGET),
        PARSER_ENTRY(OPCODE),
        PARSER_ENTRY(TARGET_OPCODE),
        PARSER_ENTRY(RS1),
        PARSER_ENTRY(RS2),
        PARSER_ENTRY(RS1_VALUE),
        PARSER_ENTRY(RS2_VALUE),
        PARSER_ENTRY(TAKEN),
        PARSER_ENTRY(COND),
        PARSER_ENTRY(CALL),
        PARSER_ENTRY(RET),
        PARSER_ENTRY(INDIRECT),
        PARSER_ENTRY(COMPARE_EQ),
        PARSER_ENTRY(COMPARE_NOT_EQ),
        PARSER_ENTRY(COMPARE_GREATER_THAN_OR_EQUAL),
        PARSER_ENTRY(COMPARE_LESS_THAN),
        PARSER_ENTRY(COMPARE_UNSIGNED),
        PARSER_ENTRY(LAST_TAKEN),
        PARSER_ENTRY(WKLD_ID)
    };

    const auto it = string_map.find(hdf5_field_str);
    stf_assert(it != string_map.end(), "Invalid HDF5 field specified: " << hdf5_field_str);
    return it->second;
}

#undef PARSER_ENTRY

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        std::string& output,
                        bool& skip_non_user,
                        bool& use_unsigned_bool,
                        bool& always_fill_in_target_opcode,
                        bool& byte_chunks,
                        std::unordered_set<HDF5Field>& excluded_fields,
                        size_t& limit_top_branches,
                        int32_t& wkld_id) {
    trace_tools::CommandLineParser parser("stf_branch_hdf5");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('l', "N", "limit output to the top N most frequent branches");
    parser.addFlag('U', "use unsigned ({0,1}) encoding for boolean values. The default behavior encodes boolean values to {-1,1}. The taken field is *always* encoded to {0,1}.");
    parser.addFlag('O', "fill in target opcodes for not-taken branches. If the opcode is unknown, it will be set to a random value.");
    parser.addFlag('1', "break up opcodes, register values, and addresses into 1-byte chunks");
    parser.addFlag('w', "wkld_id", "Add a wkld_id field with the specified value");
    parser.addMultiFlag('x', "exclude_field", "exclude specified field. Can be specified multiple times.");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.addPositionalArgument("output", "output HDF5");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');
    use_unsigned_bool = parser.hasArgument('U');
    always_fill_in_target_opcode = parser.hasArgument('O');
    parser.getArgumentValue('l', limit_top_branches);
    byte_chunks = parser.hasArgument('1');
    wkld_id = -1;
    parser.getArgumentValue('w', wkld_id);
    for(const auto& field: parser.getMultipleValueArgument('x')) {
        excluded_fields.insert(parseHDF5Field(field));
    }

    if(wkld_id == -1) {
        excluded_fields.insert(HDF5Field::WKLD_ID);
    }

    parser.getPositionalArgument(0, trace);
    parser.getPositionalArgument(1, output);
}

class OpcodeMap {
    private:
        const bool return_random_for_unknown_ = false;
        std::unordered_map<uint64_t, uint32_t> opcodes_;
        mutable std::default_random_engine rng_;
        mutable std::uniform_int_distribution<uint32_t> distribution_{1, std::numeric_limits<uint32_t>::max()};

    public:
        explicit OpcodeMap(const bool return_random_for_unknown) :
            return_random_for_unknown_(return_random_for_unknown)
        {
        }

        void updateOpcode(const uint64_t pc, const uint32_t opcode) {
            if(opcode != 0) {
                opcodes_[pc] = opcode;
            }
        }

        uint32_t getOpcode(const uint64_t pc) const {
            const auto it = opcodes_.find(pc);

            if(it == opcodes_.end()) {
                if(return_random_for_unknown_) {
                    return distribution_(rng_);
                }
                else {
                    return 0;
                }
            }

            return it->second;
        }
};

template<bool use_unsigned_bool>
struct HDF5BranchBase {
    using BoolType = std::conditional_t<use_unsigned_bool, bool, int8_t>;

    static const BoolType False;

    static const BoolType True;

    static inline BoolType encodeBool(const bool val) {
        return val ? True : False;
    }

    static inline int16_t encodeRegNum(const stf::Registers::STF_REG reg) {
        return reg == stf::Registers::STF_REG::STF_REG_INVALID ? -1 : static_cast<int16_t>(stf::Registers::Codec::packRegNum(reg));
    }

    static inline uint32_t getTargetOpcode(const stf::STFBranch& branch, const OpcodeMap& opcode_map) {
        uint32_t opcode = branch.getTargetOpcode();
        if(!opcode) {
            opcode = opcode_map.getOpcode(branch.getTargetPC());
        }

        return opcode;
    }
};

template<bool use_unsigned_bool>
struct HDF5Branch : public HDF5BranchBase<use_unsigned_bool> {
    using BoolType = typename HDF5BranchBase<use_unsigned_bool>::BoolType;
    using HDF5BranchBase<use_unsigned_bool>::False;
    using HDF5BranchBase<use_unsigned_bool>::True;
    using HDF5BranchBase<use_unsigned_bool>::encodeBool;
    using HDF5BranchBase<use_unsigned_bool>::encodeRegNum;
    using HDF5BranchBase<use_unsigned_bool>::getTargetOpcode;

    uint64_t index = 0;
    uint64_t pc = 0;
    uint64_t target = 0;
    uint32_t opcode = 0;
    uint32_t target_opcode = 0;
    int16_t rs1 = -1;
    int16_t rs2 = -1;
    uint64_t rs1_value = 0;
    uint64_t rs2_value = 0;
    bool taken = false;
    BoolType cond = False;
    BoolType call = False;
    BoolType ret = False;
    BoolType indirect = False;
    BoolType compare_eq = False;
    BoolType compare_not_eq = False;
    BoolType compare_greater_than_or_equal = False;
    BoolType compare_less_than = False;
    BoolType compare_unsigned = False;
    BoolType last_taken = 0;
    int32_t wkld_id = -1;

    HDF5Branch() = default;

    explicit HDF5Branch(const stf::STFBranch& branch, const OpcodeMap& opcode_map, const int32_t cur_wkld_id) :
        index(branch.index()),
        pc(branch.getPC()),
        target(branch.getTargetPC()),
        opcode(branch.getOpcode()),
        target_opcode(getTargetOpcode(branch, opcode_map)),
        rs1(encodeRegNum(branch.getRS1())),
        rs2(encodeRegNum(branch.getRS2())),
        rs1_value(branch.getRS1Value()),
        rs2_value(branch.getRS2Value()),
        taken(branch.isTaken()),
        cond(encodeBool(branch.isConditional())),
        call(encodeBool(branch.isCall())),
        ret(encodeBool(branch.isReturn())),
        indirect(encodeBool(branch.isIndirect())),
        compare_eq(encodeBool(branch.isCompareEqual())),
        compare_not_eq(encodeBool(branch.isCompareNotEqual())),
        compare_greater_than_or_equal(encodeBool(branch.isCompareGreaterThanOrEqual())),
        compare_less_than(encodeBool(branch.isCompareLessThan())),
        compare_unsigned(encodeBool(branch.isCompareUnsigned())),
        wkld_id(cur_wkld_id)
    {
    }

    static H5::CompType initBranchType(const std::unordered_set<HDF5Field>& excluded_fields) {
        static const auto& BoolType = use_unsigned_bool ? H5::PredType::NATIVE_HBOOL : H5::PredType::NATIVE_INT8;

        H5::CompType branch_type(sizeof(HDF5Branch));

        if(!excluded_fields.count(HDF5Field::INDEX)) {
            branch_type.insertMember("index", HOFFSET(HDF5Branch, index), H5::PredType::NATIVE_UINT64);
        }
        if(!excluded_fields.count(HDF5Field::PC)) {
            branch_type.insertMember("pc", HOFFSET(HDF5Branch, pc), H5::PredType::NATIVE_UINT64);
        }
        if(!excluded_fields.count(HDF5Field::TARGET)) {
            branch_type.insertMember("target", HOFFSET(HDF5Branch, target), H5::PredType::NATIVE_UINT64);
        }
        if(!excluded_fields.count(HDF5Field::OPCODE)) {
            branch_type.insertMember("opcode", HOFFSET(HDF5Branch, opcode), H5::PredType::NATIVE_UINT32);
        }
        if(!excluded_fields.count(HDF5Field::TARGET_OPCODE)) {
            branch_type.insertMember("target_opcode", HOFFSET(HDF5Branch, target_opcode), H5::PredType::NATIVE_UINT32);
        }
        if(!excluded_fields.count(HDF5Field::RS1)) {
            branch_type.insertMember("rs1", HOFFSET(HDF5Branch, rs1), H5::PredType::NATIVE_INT16);
        }
        if(!excluded_fields.count(HDF5Field::RS2)) {
            branch_type.insertMember("rs2", HOFFSET(HDF5Branch, rs2), H5::PredType::NATIVE_INT16);
        }
        if(!excluded_fields.count(HDF5Field::RS1_VALUE)) {
            branch_type.insertMember("rs1_value", HOFFSET(HDF5Branch, rs1_value), H5::PredType::NATIVE_UINT64);
        }
        if(!excluded_fields.count(HDF5Field::RS2_VALUE)) {
            branch_type.insertMember("rs2_value", HOFFSET(HDF5Branch, rs2_value), H5::PredType::NATIVE_UINT64);
        }
        if(!excluded_fields.count(HDF5Field::TAKEN)) {
            branch_type.insertMember("taken", HOFFSET(HDF5Branch, taken), H5::PredType::NATIVE_HBOOL);
        }
        if(!excluded_fields.count(HDF5Field::COND)) {
            branch_type.insertMember("cond", HOFFSET(HDF5Branch, cond), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::CALL)) {
            branch_type.insertMember("call", HOFFSET(HDF5Branch, call), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::RET)) {
            branch_type.insertMember("ret", HOFFSET(HDF5Branch, ret), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::INDIRECT)) {
            branch_type.insertMember("indirect", HOFFSET(HDF5Branch, indirect), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_EQ)) {
            branch_type.insertMember("compare_eq", HOFFSET(HDF5Branch, compare_eq), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_NOT_EQ)) {
            branch_type.insertMember("compare_not_eq", HOFFSET(HDF5Branch, compare_not_eq), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_GREATER_THAN_OR_EQUAL)) {
            branch_type.insertMember("compare_greater_than_or_equal", HOFFSET(HDF5Branch, compare_greater_than_or_equal), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_LESS_THAN)) {
            branch_type.insertMember("compare_less_than", HOFFSET(HDF5Branch, compare_less_than), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_UNSIGNED)) {
            branch_type.insertMember("compare_unsigned", HOFFSET(HDF5Branch, compare_unsigned), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::LAST_TAKEN)) {
            branch_type.insertMember("last_taken", HOFFSET(HDF5Branch, last_taken), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::WKLD_ID)) {
            branch_type.insertMember("wkld_id", HOFFSET(HDF5Branch, wkld_id), H5::PredType::NATIVE_INT32);
        }

        return branch_type;
    }
};

template<bool use_unsigned_bool>
struct HDF5BranchByteChunked : public HDF5BranchBase<use_unsigned_bool> {
    using BoolType = typename HDF5BranchBase<use_unsigned_bool>::BoolType;
    using HDF5BranchBase<use_unsigned_bool>::False;
    using HDF5BranchBase<use_unsigned_bool>::True;
    using HDF5BranchBase<use_unsigned_bool>::encodeBool;
    using HDF5BranchBase<use_unsigned_bool>::encodeRegNum;
    using HDF5BranchBase<use_unsigned_bool>::getTargetOpcode;

    uint64_t index = 0;
    uint8_t pc[8]{0,0,0,0,0,0,0,0};
    uint8_t target[8]{0,0,0,0,0,0,0,0};
    uint8_t opcode[4]{0,0,0,0};
    uint8_t target_opcode[4]{0,0,0,0};
    int16_t rs1 = -1;
    int16_t rs2 = -1;
    uint8_t rs1_value[8]{0,0,0,0,0,0,0,0};
    uint8_t rs2_value[8]{0,0,0,0,0,0,0,0};
    bool taken = false;
    BoolType cond = False;
    BoolType call = False;
    BoolType ret = False;
    BoolType indirect = False;
    BoolType compare_eq = False;
    BoolType compare_not_eq = False;
    BoolType compare_greater_than_or_equal = False;
    BoolType compare_less_than = False;
    BoolType compare_unsigned = False;
    BoolType last_taken = 0;
    int32_t wkld_id = -1;

    HDF5BranchByteChunked() = default;

    template<size_t N, typename T>
    static inline void initValueArray(T val, uint8_t target[N]) {
        static constexpr T byte_mask = stf::byte_utils::bitMask<T, 8>();
        for(size_t i = 0; i < N; ++i) {
            target[i] = val & byte_mask;
            val >>= 8;
        }
    }

    explicit HDF5BranchByteChunked(const stf::STFBranch& branch, const OpcodeMap& opcode_map, const int32_t cur_wkld_id) :
        index(branch.index()),
        rs1(encodeRegNum(branch.getRS1())),
        rs2(encodeRegNum(branch.getRS2())),
        taken(branch.isTaken()),
        cond(encodeBool(branch.isConditional())),
        call(encodeBool(branch.isCall())),
        ret(encodeBool(branch.isReturn())),
        indirect(encodeBool(branch.isIndirect())),
        compare_eq(encodeBool(branch.isCompareEqual())),
        compare_not_eq(encodeBool(branch.isCompareNotEqual())),
        compare_greater_than_or_equal(encodeBool(branch.isCompareGreaterThanOrEqual())),
        compare_less_than(encodeBool(branch.isCompareLessThan())),
        compare_unsigned(encodeBool(branch.isCompareUnsigned())),
        wkld_id(cur_wkld_id)
    {
        initValueArray<8>(branch.getPC(), pc);
        initValueArray<8>(branch.getTargetPC(), target);
        initValueArray<4>(branch.getOpcode(), opcode);
        initValueArray<4>(getTargetOpcode(branch, opcode_map), target_opcode);
        initValueArray<8>(branch.getRS1Value(), rs1_value);
        initValueArray<8>(branch.getRS2Value(), rs2_value);
    }

    static H5::CompType initBranchType(const std::unordered_set<HDF5Field>& excluded_fields) {
        static const auto& BoolType = use_unsigned_bool ? H5::PredType::NATIVE_HBOOL : H5::PredType::NATIVE_INT8;

        H5::CompType branch_type(sizeof(HDF5BranchByteChunked));

        if(!excluded_fields.count(HDF5Field::INDEX)) {
            branch_type.insertMember("index", HOFFSET(HDF5BranchByteChunked, index), H5::PredType::NATIVE_UINT64);
        }
        if(!excluded_fields.count(HDF5Field::PC)) {
            branch_type.insertMember("pc0", HOFFSET(HDF5BranchByteChunked, pc[0]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc1", HOFFSET(HDF5BranchByteChunked, pc[1]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc2", HOFFSET(HDF5BranchByteChunked, pc[2]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc3", HOFFSET(HDF5BranchByteChunked, pc[3]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc4", HOFFSET(HDF5BranchByteChunked, pc[4]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc5", HOFFSET(HDF5BranchByteChunked, pc[5]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc6", HOFFSET(HDF5BranchByteChunked, pc[6]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("pc7", HOFFSET(HDF5BranchByteChunked, pc[7]), H5::PredType::NATIVE_UINT8);
        }
        if(!excluded_fields.count(HDF5Field::TARGET)) {
            branch_type.insertMember("target0", HOFFSET(HDF5BranchByteChunked, target[0]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target1", HOFFSET(HDF5BranchByteChunked, target[1]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target2", HOFFSET(HDF5BranchByteChunked, target[2]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target3", HOFFSET(HDF5BranchByteChunked, target[3]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target4", HOFFSET(HDF5BranchByteChunked, target[4]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target5", HOFFSET(HDF5BranchByteChunked, target[5]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target6", HOFFSET(HDF5BranchByteChunked, target[6]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target7", HOFFSET(HDF5BranchByteChunked, target[7]), H5::PredType::NATIVE_UINT8);
        }
        if(!excluded_fields.count(HDF5Field::OPCODE)) {
            branch_type.insertMember("opcode0", HOFFSET(HDF5BranchByteChunked, opcode[0]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("opcode1", HOFFSET(HDF5BranchByteChunked, opcode[1]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("opcode2", HOFFSET(HDF5BranchByteChunked, opcode[2]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("opcode3", HOFFSET(HDF5BranchByteChunked, opcode[3]), H5::PredType::NATIVE_UINT8);
        }
        if(!excluded_fields.count(HDF5Field::TARGET_OPCODE)) {
            branch_type.insertMember("target_opcode0", HOFFSET(HDF5BranchByteChunked, target_opcode[0]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target_opcode1", HOFFSET(HDF5BranchByteChunked, target_opcode[1]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target_opcode2", HOFFSET(HDF5BranchByteChunked, target_opcode[2]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("target_opcode3", HOFFSET(HDF5BranchByteChunked, target_opcode[3]), H5::PredType::NATIVE_UINT8);
        }
        if(!excluded_fields.count(HDF5Field::RS1)) {
            branch_type.insertMember("rs1", HOFFSET(HDF5BranchByteChunked, rs1), H5::PredType::NATIVE_INT16);
        }
        if(!excluded_fields.count(HDF5Field::RS2)) {
            branch_type.insertMember("rs2", HOFFSET(HDF5BranchByteChunked, rs2), H5::PredType::NATIVE_INT16);
        }
        if(!excluded_fields.count(HDF5Field::RS1_VALUE)) {
            branch_type.insertMember("rs1_value0", HOFFSET(HDF5BranchByteChunked, rs1_value[0]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value1", HOFFSET(HDF5BranchByteChunked, rs1_value[1]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value2", HOFFSET(HDF5BranchByteChunked, rs1_value[2]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value3", HOFFSET(HDF5BranchByteChunked, rs1_value[3]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value4", HOFFSET(HDF5BranchByteChunked, rs1_value[4]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value5", HOFFSET(HDF5BranchByteChunked, rs1_value[5]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value6", HOFFSET(HDF5BranchByteChunked, rs1_value[6]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs1_value7", HOFFSET(HDF5BranchByteChunked, rs1_value[7]), H5::PredType::NATIVE_UINT8);
        }
        if(!excluded_fields.count(HDF5Field::RS2_VALUE)) {
            branch_type.insertMember("rs2_value0", HOFFSET(HDF5BranchByteChunked, rs2_value[0]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value1", HOFFSET(HDF5BranchByteChunked, rs2_value[1]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value2", HOFFSET(HDF5BranchByteChunked, rs2_value[2]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value3", HOFFSET(HDF5BranchByteChunked, rs2_value[3]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value4", HOFFSET(HDF5BranchByteChunked, rs2_value[4]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value5", HOFFSET(HDF5BranchByteChunked, rs2_value[5]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value6", HOFFSET(HDF5BranchByteChunked, rs2_value[6]), H5::PredType::NATIVE_UINT8);
            branch_type.insertMember("rs2_value7", HOFFSET(HDF5BranchByteChunked, rs2_value[7]), H5::PredType::NATIVE_UINT8);
        }
        if(!excluded_fields.count(HDF5Field::TAKEN)) {
            branch_type.insertMember("taken", HOFFSET(HDF5BranchByteChunked, taken), H5::PredType::NATIVE_HBOOL);
        }
        if(!excluded_fields.count(HDF5Field::COND)) {
            branch_type.insertMember("cond", HOFFSET(HDF5BranchByteChunked, cond), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::CALL)) {
            branch_type.insertMember("call", HOFFSET(HDF5BranchByteChunked, call), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::RET)) {
            branch_type.insertMember("ret", HOFFSET(HDF5BranchByteChunked, ret), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::INDIRECT)) {
            branch_type.insertMember("indirect", HOFFSET(HDF5BranchByteChunked, indirect), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_EQ)) {
            branch_type.insertMember("compare_eq", HOFFSET(HDF5BranchByteChunked, compare_eq), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_NOT_EQ)) {
            branch_type.insertMember("compare_not_eq", HOFFSET(HDF5BranchByteChunked, compare_not_eq), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_GREATER_THAN_OR_EQUAL)) {
            branch_type.insertMember("compare_greater_than_or_equal", HOFFSET(HDF5BranchByteChunked, compare_greater_than_or_equal), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_LESS_THAN)) {
            branch_type.insertMember("compare_less_than", HOFFSET(HDF5BranchByteChunked, compare_less_than), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::COMPARE_UNSIGNED)) {
            branch_type.insertMember("compare_unsigned", HOFFSET(HDF5BranchByteChunked, compare_unsigned), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::LAST_TAKEN)) {
            branch_type.insertMember("last_taken", HOFFSET(HDF5BranchByteChunked, last_taken), BoolType);
        }
        if(!excluded_fields.count(HDF5Field::WKLD_ID)) {
            branch_type.insertMember("wkld_id", HOFFSET(HDF5BranchByteChunked, wkld_id), H5::PredType::NATIVE_INT32);
        }

        return branch_type;
    }
};

template<>
const HDF5BranchBase<true>::BoolType HDF5BranchBase<true>::False = false;

template<>
const HDF5BranchBase<true>::BoolType HDF5BranchBase<true>::True = true;

template<>
const HDF5BranchBase<false>::BoolType HDF5BranchBase<false>::False = -1;

template<>
const HDF5BranchBase<false>::BoolType HDF5BranchBase<false>::True = 1;

template<size_t CHUNK_SIZE, typename BranchType>
class HDF5BranchWriter {
    private:
        static constexpr int RANK_ = 1;
        static constexpr hsize_t MAX_DIMS_[1] = {H5S_UNLIMITED};
        static constexpr hsize_t CHUNK_DIM_[1] ={CHUNK_SIZE};
        static constexpr int ZLIB_COMPRESSION_LEVEL = 7;
        static inline const H5std_string DATASET_NAME_{"branch_info"};

        using BufferT = std::array<BranchType, CHUNK_SIZE>;
        BufferT branch_buffer_{};
        typename BufferT::iterator it_ = branch_buffer_.begin();

        const H5::H5File hdf5_file_;
        const H5::DataSpace chunk_mspace_;
        const H5::CompType branch_type_;
        const H5::DataSet dataset_;

        hsize_t cur_slab_dim_[1] = {0};
        OpcodeMap opcode_map_;
        const int32_t wkld_id_ = -1;
        typename BranchType::BoolType last_taken_ = 0;

        inline void writeChunk_(const size_t num_items, const hsize_t chunk_dim[], const H5::DataSpace& mspace) {
            const hsize_t hyperslab_offset = cur_slab_dim_[0];
            cur_slab_dim_[0] += num_items;
            dataset_.extend(cur_slab_dim_);
            H5::DataSpace fspace = dataset_.getSpace();
            fspace.selectHyperslab(H5S_SELECT_SET, chunk_dim, &hyperslab_offset);
            dataset_.write(branch_buffer_.data(), branch_type_, mspace, fspace);
        }

        inline void writeChunk_() {
            writeChunk_(CHUNK_SIZE, CHUNK_DIM_, chunk_mspace_);
        }

        static H5::DSetCreatPropList getDataSetProps_() {
            H5::DSetCreatPropList cparms;
            cparms.setChunk(RANK_, CHUNK_DIM_);
            cparms.setShuffle();
            cparms.setDeflate(ZLIB_COMPRESSION_LEVEL);
            return cparms;
        }

    public:
        explicit HDF5BranchWriter(const std::string& filename, const bool return_random_for_unknown_target_opcode, const std::unordered_set<HDF5Field>& excluded_fields, const int32_t wkld_id) :
            hdf5_file_(filename.c_str(), H5F_ACC_TRUNC),
            chunk_mspace_(RANK_, CHUNK_DIM_, MAX_DIMS_),
            branch_type_(BranchType::initBranchType(excluded_fields)),
            dataset_(hdf5_file_.createDataSet(DATASET_NAME_, branch_type_, chunk_mspace_, getDataSetProps_())),
            opcode_map_(return_random_for_unknown_target_opcode),
            wkld_id_(wkld_id)
        {
        }

        ~HDF5BranchWriter() {
            if(it_ != branch_buffer_.begin()) {
                const auto num_items = static_cast<size_t>(std::distance(branch_buffer_.begin(), it_));
                const hsize_t dim[1] = {num_items};
                H5::DataSpace mspace(RANK_, dim, MAX_DIMS_);
                writeChunk_(num_items, dim, mspace);
            }
        }

        inline void append(const stf::STFBranch& branch) {
            opcode_map_.updateOpcode(branch.getTargetPC(), branch.getTargetOpcode());
            *it_ = BranchType(branch, opcode_map_, wkld_id_);
            it_->last_taken = last_taken_;
            last_taken_ = it_->taken;
            ++it_;
            if(it_ == branch_buffer_.end()) {
                writeChunk_();
                it_ = branch_buffer_.begin();
            }
        }
};

template<bool byte_chunks, bool use_unsigned_bool>
struct BranchTypeChooser;

template<bool use_unsigned_bool>
struct BranchTypeChooser<false, use_unsigned_bool> {
    using type = HDF5Branch<use_unsigned_bool>;
};

template<bool use_unsigned_bool>
struct BranchTypeChooser<true, use_unsigned_bool> {
    using type = HDF5BranchByteChunked<use_unsigned_bool>;
};

template<bool use_unsigned_bool, bool byte_chunks>
void processTrace(const std::string& trace,
                  const std::string& output,
                  const bool skip_non_user,
                  const bool always_fill_in_target_opcode,
                  const std::set<uint64_t>& top_branches,
                  const std::unordered_set<HDF5Field>& excluded_fields,
                  const int32_t wkld_id) {
    static constexpr size_t CHUNK_SIZE = 1000;

    stf::STFBranchReader reader(trace, skip_non_user);
    HDF5BranchWriter<CHUNK_SIZE, typename BranchTypeChooser<byte_chunks, use_unsigned_bool>::type> writer(output, always_fill_in_target_opcode, excluded_fields, wkld_id);

    if(top_branches.empty()) {
        for(const auto& branch: reader) {
            writer.append(branch);
        }
    }
    else {
        for(const auto& branch: reader) {
            if(top_branches.count(branch.getPC())) {
                writer.append(branch);
            }
        }
    }
}

std::set<uint64_t> getTopBranches(const std::string& trace, const bool skip_non_user, const size_t limit_top_branches) {
    std::set<uint64_t> top_branches;

    if(limit_top_branches) {
        std::unordered_map<uint64_t, uint64_t> branch_counts;
        stf::STFBranchReader reader(trace, skip_non_user);
        for(const auto& branch: reader) {
            ++branch_counts[branch.getPC()];
        }

        std::multimap<uint64_t, uint64_t> sorted_branches;
        for(const auto& p: branch_counts) {
            sorted_branches.emplace(p.second, p.first);
        }

        for(auto it = sorted_branches.rbegin(); it != sorted_branches.rend(); ++it) {
            top_branches.emplace(it->second);

            if(top_branches.size() == limit_top_branches) {
                break;
            }
        }
    }

    return top_branches;
}

int main(int argc, char** argv) {
    std::string trace;
    std::string output;
    bool skip_non_user = false;
    bool use_unsigned_bool = false;
    bool always_fill_in_target_opcode = false;
    bool byte_chunks = false;
    std::unordered_set<HDF5Field> excluded_fields;
    size_t limit_top_branches = 0;
    int32_t wkld_id = -1;

    try {
        processCommandLine(argc,
                           argv,
                           trace,
                           output,
                           skip_non_user,
                           use_unsigned_bool,
                           always_fill_in_target_opcode,
                           byte_chunks,
                           excluded_fields,
                           limit_top_branches,
                           wkld_id);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    std::set<uint64_t> top_branches = getTopBranches(trace, skip_non_user, limit_top_branches);

    if(use_unsigned_bool) {
        if(byte_chunks) {
            processTrace<true, true>(trace, output, skip_non_user, always_fill_in_target_opcode, top_branches, excluded_fields, wkld_id);
        }
        else {
            processTrace<true, false>(trace, output, skip_non_user, always_fill_in_target_opcode, top_branches, excluded_fields, wkld_id);
        }
    }
    else {
        if(byte_chunks) {
            processTrace<false, true>(trace, output, skip_non_user, always_fill_in_target_opcode, top_branches, excluded_fields, wkld_id);
        }
        else {
            processTrace<false, false>(trace, output, skip_non_user, always_fill_in_target_opcode, top_branches, excluded_fields, wkld_id);
        }
    }

    return 0;
}
