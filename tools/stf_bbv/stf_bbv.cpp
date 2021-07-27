/**
 * \brief  This tool prints out the content of a STF trace file
 *
 */

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include "stf_inst_reader.hpp"

#include "command_line_parser.hpp"
#include "file_utils.hpp"

static void parseCommandLine(int argc,
                             char **argv,
                             std::string& trace_filename,
                             std::string& output_filename,
                             std::string& user_mode_filename,
                             uint64_t& start_inst,
                             uint64_t& end_inst,
                             uint64_t& interval,
                             uint64_t& min_user_insts) {
    trace_tools::CommandLineParser parser("stf_bbv");
    parser.addFlag('o', "output", "output filename (defaults to stdout if omitted)");
    parser.addFlag('u', "user_mode_file", "output filename containing whether each interval has non-user code in it");
    parser.addFlag('s', "N", "start to collect Basic Block Vector info at N-th instruction");
    parser.addFlag('e', "M", "end basic block vector collection at M-th instruction");
    parser.addFlag('w', "K", "basic block vector collection in every K instructions");
    parser.addFlag('m', "L", "ensure a minimum of L instructions have passed before dumping user-mode BBVs");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);
    parser.getArgumentValue('u', user_mode_filename);
    parser.getArgumentValue('s', start_inst);
    parser.getArgumentValue('e', end_inst);
    parser.getArgumentValue('w', interval);
    parser.getArgumentValue('m', min_user_insts);
    parser.getPositionalArgument(0, trace_filename);

    parser.assertCondition(!end_inst || (end_inst <= start_inst), "End inst must be greater than start inst");
}

class BasicBlockTracker {
    public:
        struct BasicBlockRange {
            uint64_t bb_start;
            uint64_t bb_end;

            BasicBlockRange(const uint64_t s, const uint64_t e) :
                bb_start(s),
                bb_end(e)
            {
            }

            bool operator<(const BasicBlockRange& other) const {
                return (this->bb_start < other.bb_start) ? true :
                       (this->bb_start > other.bb_start) ? false :
                       (this->bb_end < other.bb_end);
            }
        };

    private:
        class BasicBlockInfo {
            friend class BasicBlockTracker;

            private:
                uint64_t bb_id_;     // block id;
                //uint64_t bb_insts_;  // instructions in block
                uint64_t bb_count_;  // block executed count;

            public:
                BasicBlockInfo(const uint64_t id, const uint64_t i, const uint64_t c) :
                    bb_id_(id),
                    //bb_insts_(i),
                    bb_count_(c)
                {
                }
        };

        using BasicBlockMap = std::map<BasicBlockRange, BasicBlockInfo>;

        const uint64_t interval_;
        const uint64_t min_user_insts_;
        OutputFileStream os_;
        OutputFileStream interval_file_;
        std::ofstream user_mode_file_;
        std::ofstream user_interval_file_;
        uint64_t last_interval_idx_ = 0;
        BasicBlockMap bbv_;

    public:
        explicit BasicBlockTracker(const uint64_t interval, const uint64_t min_user_insts, const std::string& output_filename, const std::string& user_mode_filename) :
            interval_(interval),
            min_user_insts_(min_user_insts),
            os_(output_filename),
            interval_file_(output_filename != "-" ? output_filename + ".interval" : "-")
        {
            if(!user_mode_filename.empty()) {
                user_mode_file_.open(user_mode_filename, std::ofstream::trunc);
                user_interval_file_.open(user_mode_filename + ".interval", std::ofstream::trunc);
            }
        }

        void updateBasicBlockVector(BasicBlockRange &bbr, uint64_t& instcnt, uint64_t& interval_count, bool& has_non_user_code, const uint64_t inst_idx, const bool dump_interval = true) {
            if(bbr.bb_start == 0) {
                return;
            }

            auto it = bbv_.find(bbr);
            if(it == bbv_.end()) {
                bbv_.emplace_hint(it,
                                  std::piecewise_construct,
                                  std::forward_as_tuple(bbr),
                                  std::forward_as_tuple(bbv_.size()+1, instcnt, instcnt));
            }
            else {
                it->second.bb_count_ += instcnt;
            }

            bbr.bb_start = 0;
            bbr.bb_end = 0;

            instcnt = 0;
            if(interval_count >= interval_) {
                dumpBasicBlockVector(interval_count, has_non_user_code, inst_idx, dump_interval);
                interval_count = 0;
                has_non_user_code = false;
            }
        }

        void dumpBasicBlockVector(const uint64_t interval_count, const bool has_non_user_code, const uint64_t inst_idx, const bool dump_interval) {
            if(interval_count) {
                std::ostringstream ss;
                ss << 'T';
                for(auto& b: bbv_) {
                    if(b.second.bb_count_) {
                        ss << ':' << b.second.bb_id_ << ':' << b.second.bb_count_ << ' ';
                        b.second.bb_count_ = 0;
                    }
                }
                ss << std::endl;

                const auto str = ss.str();
                os_ << str;

                if(user_mode_file_) {
                    if(has_non_user_code || ((inst_idx != std::numeric_limits<uint64_t>::max()) && ((inst_idx - interval_count) < min_user_insts_))) {
                        user_mode_file_ << std::endl;
                    }
                    else {
                        user_mode_file_ << str;
                        if(dump_interval) {
                            user_interval_file_ << last_interval_idx_ << std::endl;
                        }
                    }
                }

                if(dump_interval) {
                    interval_file_ << last_interval_idx_ << std::endl;
                    last_interval_idx_ = inst_idx;
                }
            }
        }
};

int main(int argc, char **argv) {
    static constexpr uint64_t DEFAULT_INTERVAL = 100000000;

    std::string trace_filename;
    std::string output_filename = "-";
    std::string user_mode_filename;
    uint64_t start_inst = 0;
    uint64_t end_inst = 0;
    uint64_t interval = DEFAULT_INTERVAL;
    uint64_t min_user_insts = 0;

    try {
        parseCommandLine(argc, argv, trace_filename, output_filename, user_mode_filename, start_inst, end_inst, interval, min_user_insts);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    if(!end_inst) {
        end_inst = std::numeric_limits<uint64_t>::max();
    }

    // Open stf trace reader
    stf::STFInstReader stf_reader(trace_filename);
    /* FIXME Because we have not kept up with STF versioning, this is currently broken and must be loosened.
    if (!stf_reader.checkVersion()) {
        exit(1);
    }
    */

    uint64_t cur_bb_count = 0;
    BasicBlockTracker::BasicBlockRange cur_bbr(0,0);
    uint64_t interval_count = 0;

    BasicBlockTracker tracker(interval, min_user_insts, output_filename, user_mode_filename);
    bool has_non_user_code = false;

    for(const auto& inst: stf_reader) {
        if(STF_EXPECT_FALSE(!inst.valid())) {
            continue;
        }

        if(STF_EXPECT_FALSE(inst.index() < start_inst)) {
            continue;
        }

        if(STF_EXPECT_FALSE(inst.index() > end_inst)) {
            break;
        }

        has_non_user_code |= inst.isChangeFromUserMode();

        if(STF_EXPECT_FALSE(inst.isCoF())) {
            tracker.updateBasicBlockVector(cur_bbr, cur_bb_count, interval_count, has_non_user_code, inst.index(), false);
        }

        if(!cur_bbr.bb_start) {
            cur_bbr.bb_start = inst.pc();
            cur_bbr.bb_end = cur_bbr.bb_start;
        }

        cur_bbr.bb_end += inst.opcodeSize();
        cur_bb_count++;
        interval_count++;

        if(STF_EXPECT_FALSE(inst.isTakenBranch() || !inst.getEvents().empty())) {
            tracker.updateBasicBlockVector(cur_bbr, cur_bb_count, interval_count, has_non_user_code, inst.index());
        }
    }

    tracker.dumpBasicBlockVector(interval_count, has_non_user_code, std::numeric_limits<uint64_t>::max(), true);

    return 0;
}

