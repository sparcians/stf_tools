#include <array>
#include <iostream>
#include <map>
#include <unordered_map>
#include <H5Cpp.h>

#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        std::string& output,
                        bool& skip_non_user,
                        bool& use_unsigned_bool,
                        size_t& limit_top_branches) {
    trace_tools::CommandLineParser parser("stf_branch_hdf5");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('l', "N", "limit output to the top N most frequent branches");
    parser.addFlag('U', "use unsigned ({0,1}) encoding for boolean values. The default behavior encodes boolean values to {-1,1}. The taken field is *always* encoded to {0,1}.");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.addPositionalArgument("output", "output HDF5");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');
    use_unsigned_bool = parser.hasArgument('U');
    parser.getArgumentValue('l', limit_top_branches);

    parser.getPositionalArgument(0, trace);
    parser.getPositionalArgument(1, output);
}

template<bool use_unsigned_bool>
struct HDF5Branch {
    using BoolType = std::conditional_t<use_unsigned_bool, bool, int8_t>;

    static const BoolType False;

    static const BoolType True;

    static inline BoolType encodeBool(const bool val) {
        return val ? True : False;
    }

    uint64_t index = 0;
    uint64_t pc = 0;
    uint64_t target = 0;
    uint32_t opcode = 0;
    uint32_t target_opcode = 0;
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

    HDF5Branch() = default;

    explicit HDF5Branch(const stf::STFBranch& branch) :
        index(branch.index()),
        pc(branch.getPC()),
        target(branch.getTargetPC()),
        opcode(branch.getOpcode()),
        target_opcode(branch.getTargetOpcode()),
        taken(branch.isTaken()),
        cond(encodeBool(branch.isConditional())),
        call(encodeBool(branch.isCall())),
        ret(encodeBool(branch.isReturn())),
        indirect(encodeBool(branch.isIndirect())),
        compare_eq(encodeBool(branch.isCompareEqual())),
        compare_not_eq(encodeBool(branch.isCompareNotEqual())),
        compare_greater_than_or_equal(encodeBool(branch.isCompareGreaterThanOrEqual())),
        compare_less_than(encodeBool(branch.isCompareLessThan())),
        compare_unsigned(encodeBool(branch.isCompareUnsigned()))
    {
    }
};

template<>
const HDF5Branch<true>::BoolType HDF5Branch<true>::False = false;

template<>
const HDF5Branch<true>::BoolType HDF5Branch<true>::True = true;

template<>
const HDF5Branch<false>::BoolType HDF5Branch<false>::False = -1;

template<>
const HDF5Branch<false>::BoolType HDF5Branch<false>::True = 1;

template<size_t CHUNK_SIZE, bool use_unsigned_bool>
class HDF5BranchWriter {
    private:
        static constexpr int RANK_ = 1;
        static constexpr hsize_t MAX_DIMS_[1] = {H5S_UNLIMITED};
        static constexpr hsize_t CHUNK_DIM_[1] ={CHUNK_SIZE};
        static constexpr int ZLIB_COMPRESSION_LEVEL = 7;
        static inline const H5std_string DATASET_NAME_{"branch_info"};

        using BranchType = HDF5Branch<use_unsigned_bool>;

        using BufferT = std::array<BranchType, CHUNK_SIZE>;
        BufferT branch_buffer_{};
        typename BufferT::iterator it_ = branch_buffer_.begin();

        const H5::H5File hdf5_file_;
        const H5::DataSpace chunk_mspace_;
        const H5::CompType branch_type_;
        const H5::DataSet dataset_;

        hsize_t cur_slab_dim_[1] = {0};

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

        static H5::CompType initBranchType_() {
            static const auto& BoolType = use_unsigned_bool ? H5::PredType::NATIVE_HBOOL : H5::PredType::NATIVE_INT8;

            H5::CompType branch_type(sizeof(BranchType));

            branch_type.insertMember("index", HOFFSET(BranchType, index), H5::PredType::NATIVE_UINT64);
            branch_type.insertMember("pc", HOFFSET(BranchType, pc), H5::PredType::NATIVE_UINT64);
            branch_type.insertMember("target", HOFFSET(BranchType, target), H5::PredType::NATIVE_UINT64);
            branch_type.insertMember("opcode", HOFFSET(BranchType, opcode), H5::PredType::NATIVE_UINT32);
            branch_type.insertMember("target_opcode", HOFFSET(BranchType, target_opcode), H5::PredType::NATIVE_UINT32);
            branch_type.insertMember("taken", HOFFSET(BranchType, taken), H5::PredType::NATIVE_HBOOL);
            branch_type.insertMember("cond", HOFFSET(BranchType, cond), BoolType);
            branch_type.insertMember("call", HOFFSET(BranchType, call), BoolType);
            branch_type.insertMember("ret", HOFFSET(BranchType, ret), BoolType);
            branch_type.insertMember("indirect", HOFFSET(BranchType, indirect), BoolType);
            branch_type.insertMember("compare_eq", HOFFSET(BranchType, compare_eq), BoolType);
            branch_type.insertMember("compare_not_eq", HOFFSET(BranchType, compare_not_eq), BoolType);
            branch_type.insertMember("compare_greater_than_or_equal", HOFFSET(BranchType, compare_greater_than_or_equal), BoolType);
            branch_type.insertMember("compare_less_than", HOFFSET(BranchType, compare_less_than), BoolType);
            branch_type.insertMember("compare_unsigned", HOFFSET(BranchType, compare_unsigned), BoolType);

            return branch_type;
        }

        static H5::DSetCreatPropList getDataSetProps_() {
            H5::DSetCreatPropList cparms;
            cparms.setChunk(RANK_, CHUNK_DIM_);
            cparms.setShuffle();
            cparms.setDeflate(ZLIB_COMPRESSION_LEVEL);
            return cparms;
        }

    public:
        explicit HDF5BranchWriter(const std::string& filename) :
            hdf5_file_(filename.c_str(), H5F_ACC_TRUNC),
            chunk_mspace_(RANK_, CHUNK_DIM_, MAX_DIMS_),
            branch_type_(initBranchType_()),
            dataset_(hdf5_file_.createDataSet(DATASET_NAME_, branch_type_, chunk_mspace_, getDataSetProps_()))
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
            *(it_++) = BranchType(branch);
            if(it_ == branch_buffer_.end()) {
                writeChunk_();
                it_ = branch_buffer_.begin();
            }
        }
};

template<bool use_unsigned_bool>
void processTrace(const std::string& trace,
                  const std::string& output,
                  const bool skip_non_user,
                  const std::set<uint64_t>& top_branches) {
    static constexpr size_t CHUNK_SIZE = 1000;

    stf::STFBranchReader reader(trace, skip_non_user);
    HDF5BranchWriter<CHUNK_SIZE, use_unsigned_bool> writer(output);

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
    size_t limit_top_branches = 0;

    try {
        processCommandLine(argc, argv, trace, output, skip_non_user, use_unsigned_bool, limit_top_branches);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    std::set<uint64_t> top_branches = getTopBranches(trace, skip_non_user, limit_top_branches);

    if(use_unsigned_bool) {
        processTrace<true>(trace, output, skip_non_user, top_branches);
    }
    else {
        processTrace<false>(trace, output, skip_non_user, top_branches);
    }

    return 0;
}
