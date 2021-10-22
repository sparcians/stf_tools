#include <array>
#include <iostream>
#include <H5Cpp.h>

#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        std::string& output,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_branch_hdf5");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.addPositionalArgument("output", "output HDF5");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, trace);
    parser.getPositionalArgument(1, output);
}

struct HDF5Branch {
    uint64_t index = 0;
    uint64_t pc = 0;
    uint64_t target = 0;
    uint32_t opcode = 0;
    uint32_t target_opcode = 0;
    bool taken = false;
    bool cond = false;
    bool call = false;
    bool ret = false;
    bool indirect = false;

    HDF5Branch() = default;

    explicit HDF5Branch(const stf::STFBranch& branch) :
        index(branch.index()),
        pc(branch.getPC()),
        target(branch.getTargetPC()),
        opcode(branch.getOpcode()),
        target_opcode(branch.getTargetOpcode()),
        cond(branch.isConditional()),
        call(branch.isCall()),
        ret(branch.isReturn()),
        indirect(branch.isIndirect())
    {
    }
};

template<size_t CHUNK_SIZE>
class HDF5BranchWriter {
    private:
        static constexpr int RANK_ = 1;
        static constexpr hsize_t MAX_DIMS_[1] = {H5S_UNLIMITED};
        static constexpr hsize_t CHUNK_DIM_[1] ={CHUNK_SIZE};
        static constexpr int ZLIB_COMPRESSION_LEVEL = 7;
        static inline const H5std_string DATASET_NAME_{"branch_info"};

        using BufferT = std::array<HDF5Branch, CHUNK_SIZE>;
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
            H5::CompType branch_type(sizeof(HDF5Branch));
            branch_type.insertMember("index", HOFFSET(HDF5Branch, index), H5::PredType::NATIVE_UINT64);
            branch_type.insertMember("pc", HOFFSET(HDF5Branch, pc), H5::PredType::NATIVE_UINT64);
            branch_type.insertMember("target", HOFFSET(HDF5Branch, target), H5::PredType::NATIVE_UINT64);
            branch_type.insertMember("opcode", HOFFSET(HDF5Branch, opcode), H5::PredType::NATIVE_UINT32);
            branch_type.insertMember("target_opcode", HOFFSET(HDF5Branch, target_opcode), H5::PredType::NATIVE_UINT32);
            branch_type.insertMember("taken", HOFFSET(HDF5Branch, taken), H5::PredType::NATIVE_HBOOL);
            branch_type.insertMember("cond", HOFFSET(HDF5Branch, cond), H5::PredType::NATIVE_HBOOL);
            branch_type.insertMember("call", HOFFSET(HDF5Branch, call), H5::PredType::NATIVE_HBOOL);
            branch_type.insertMember("ret", HOFFSET(HDF5Branch, ret), H5::PredType::NATIVE_HBOOL);
            branch_type.insertMember("indirect", HOFFSET(HDF5Branch, indirect), H5::PredType::NATIVE_HBOOL);

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
            *(it_++) = HDF5Branch(branch);
            if(it_ == branch_buffer_.end()) {
                writeChunk_();
                it_ = branch_buffer_.begin();
            }
        }
};

int main(int argc, char** argv) {
    std::string trace;
    std::string output;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, output, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    static constexpr size_t CHUNK_SIZE = 1000;

    stf::STFBranchReader reader(trace, skip_non_user);
    HDF5BranchWriter<CHUNK_SIZE> writer(output);

    for(const auto& branch: reader) {
        writer.append(branch);
    }

    return 0;
}
