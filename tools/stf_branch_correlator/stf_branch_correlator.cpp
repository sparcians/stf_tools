#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <bitset>
#include <boost/format.hpp>

#include "stf_decoder.hpp"
#include "print_utils.hpp"
#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"
#include <boost/multiprecision/cpp_int.hpp>

void processCommandLine(int argc,
                        char** argv,
                        bool &skip_non_user,
                        std::string &trace_file,
                        bool &output_pair_corr) {
    trace_tools::CommandLineParser parser("stf_branch_correlator");
    parser.appendHelpText("Detect and output correlation data for static branches in trace.  "
                          "Correlation can be to global history (history correlation) or other branches (pair correlation).  "
                          "Default output is history correlation.");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('p', "Output branch pair correlation data");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    output_pair_corr = parser.hasArgument('p');
    skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, trace_file);
}

static constexpr uint32_t MAX_HISTORY = 1024;  // must match Ghr::Bits; sizeof(Ghr::Bits) doesn't work
using BranchHistory = std::bitset<MAX_HISTORY>;

struct BranchInfo {
    uint64_t pc = 0;
    uint32_t count = 0;          // number of occurence in the trace
    uint32_t total_taken = 0;    // number of times branches was taken
    bool     prev_taken = false; // direction of branch when last seen
    bool operator < (const BranchInfo& binfo) const { return binfo.count < count; } // used by std::sort
};

struct PairCorrData {
    int32_t corr_cnt = 0;  // incremented by 1 if same direction, left alone otherwise
};
struct HistCorrData {
    int32_t corr_cnt = 0;  // incremented by 1 if same direction, left alone otherwise
};

void determineStaticBranches(bool skip_non_user,
                             const std::string &trace_file,
                             std::map<uint64_t, BranchInfo> &static_branches);
void determinePairCorrelation(bool skip_non_user,
                              const std::string &trace_file,
                              std::map<uint64_t, BranchInfo> &static_branches,
                              std::map<uint64_t, std::map<uint64_t, PairCorrData>> &pair_corr_matrix);
void printPairCorrelationTable(const std::vector<BranchInfo> &sorted_static_branches,
                               const std::map<uint64_t, std::map<uint64_t, PairCorrData>> &pair_corr_matrix);
void determineHistoryCorrelation(bool skip_non_user,
                                 const std::string &trace_file,
                                 std::map<uint64_t, std::vector<HistCorrData>> &hist_corr_matrix);
void printHistoryCorrelationTable(const std::vector<BranchInfo> &sorted_static_branches,
                                  const std::map<uint64_t, std::vector<HistCorrData>> &hist_corr_matrix);

int main(int argc, char** argv)
{
    bool skip_non_user = false;
    std::string trace_file;
    bool output_pair_corr = false;

    try {
        processCommandLine(argc, argv, skip_non_user, trace_file, output_pair_corr);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    // Determine static branches
    std::map<uint64_t, BranchInfo> static_branches;
    determineStaticBranches(skip_non_user, trace_file, static_branches);

    // convert BranchInfo map to a vector, and then sort by BranchInfo::count
    std::vector<BranchInfo> sorted_static_branches;  // convert map to vector so we can sort
    std::transform(static_branches.begin(),
                   static_branches.end(),
                   std::back_inserter(sorted_static_branches),
                   [](const std::pair<uint64_t,BranchInfo> &binfo) { return binfo.second;} );
    std::sort(sorted_static_branches.begin(), sorted_static_branches.end()); // uses BranchInfo::operator<

    if (output_pair_corr) {
        // Pair Correlation
        std::map<uint64_t, std::map<uint64_t, PairCorrData>> pair_corr_matrix;  // 2D sparse array of corr-count
        determinePairCorrelation(skip_non_user, trace_file, static_branches,pair_corr_matrix);
        printPairCorrelationTable(sorted_static_branches, pair_corr_matrix);
    }
    else {
        // History Correlation
        std::map<uint64_t, std::vector<HistCorrData>> hist_corr_matrix;  // 2D sparse array of corr-count
        determineHistoryCorrelation(skip_non_user, trace_file, hist_corr_matrix);
        printHistoryCorrelationTable(sorted_static_branches, hist_corr_matrix);
    }

    return 0;

}

void determineStaticBranches(bool skip_non_user,
                             const std::string &trace_file,
                             std::map<uint64_t, BranchInfo> &static_branches)
{
    stf::STFBranchReader reader(trace_file, skip_non_user);

    // Detect and count all conditional branches and populate static_branches
    for(const auto& branch: reader) {
        if (branch.isConditional()) {
            const auto pc = branch.getPC();
            auto& branch_info = static_branches[pc];
            branch_info.pc = pc;
            ++branch_info.count;
            if (branch.isTaken()) {
                ++branch_info.total_taken;
            }
        }
    }
    reader.close();
}

void determinePairCorrelation(bool skip_non_user,
                              const std::string &trace_file,
                              std::map<uint64_t, BranchInfo> &static_branches,
                              std::map<uint64_t, std::map<uint64_t, PairCorrData>> &pair_corr_matrix)
{
    stf::STFBranchReader reader(trace_file, skip_non_user);

    // Branch Pair correlation:  how a the current-branch correlates to last-taken of all other branches
    for(const auto& branch: reader) {
        if (branch.isConditional()) {
            const auto &current_pc = branch.getPC();
            const auto current_taken = branch.isTaken();
            auto &pair_corr_row = pair_corr_matrix[current_pc];
            for(const auto& itr: static_branches) {
                auto &binfo2 = itr.second;
                PairCorrData &corr_data = pair_corr_row[binfo2.pc];
                if (current_taken == binfo2.prev_taken) {
                    ++corr_data.corr_cnt;
                }
            }
            static_branches.at(current_pc).prev_taken = current_taken;
        }
    }
    reader.close();
}

void
printPairCorrelationTable(const std::vector<BranchInfo> &sorted_static_branches,
                          const std::map<uint64_t, std::map<uint64_t, PairCorrData>> &pair_corr_matrix)
{
    // Display header
    std::cout << "Branch Pair Correlation Table" << std::endl;
    std::cout << "br_pc,count,total_taken";
    for(const auto& branch: sorted_static_branches) {
        std::cout << "," << std::hex << branch.pc;
    }
    std::cout << std::endl;

    // Branch Pair Correlation table
    for(const auto& branch: sorted_static_branches) {
        std::cout << std::hex << branch.pc
                  << "," << std::dec << branch.count
                  << "," << branch.total_taken;
        const auto itr = pair_corr_matrix.find(branch.pc);

        stf_assert(itr != pair_corr_matrix.end(), "Not found pc=" << std::hex << branch.pc);

        const auto &pair_corr_row = itr->second;
        for(const auto& branch2: sorted_static_branches) {
            const auto itr2 = pair_corr_row.find(branch2.pc);
            stf_assert(itr2 != pair_corr_row.end(),  "Not found  pc=" << std::hex << branch2.pc );
            const PairCorrData &cdata = itr2->second;
            std::cout << "," << boost::format("%0.3f") % (double(cdata.corr_cnt)/branch.count);
        }
        std::cout << std::endl;
    }
}

void
determineHistoryCorrelation(bool skip_non_user,
                            const std::string &trace_file,
                            std::map<uint64_t, std::vector<HistCorrData>> &hist_corr_matrix)
{
    stf::STFBranchReader reader(trace_file, skip_non_user);
    BranchHistory ghist{0};

    // Branch History correlation:  how a the current-branch correlates to the global history
    for(const auto& branch: reader) {
        const auto current_taken = branch.isTaken();
        if (branch.isConditional()) {
            const auto &current_pc = branch.getPC();
            auto &hist_corr_row = hist_corr_matrix[current_pc];
            if (hist_corr_row.size() == 0) {
                hist_corr_row.resize(MAX_HISTORY);
            }
            for(uint32_t idx=0; idx<MAX_HISTORY; ++idx) {
                HistCorrData &corr = hist_corr_row[idx];
                if (current_taken == ghist[idx]) {
                    ++corr.corr_cnt;
                }
            }
        }
        // global history includes ALL branches
        ghist <<= 1;
        ghist |= current_taken;
    }
    reader.close();

}

void
printHistoryCorrelationTable(const std::vector<BranchInfo> &sorted_static_branches,
                             const std::map<uint64_t, std::vector<HistCorrData>> &hist_corr_matrix)
{
    std::cout << "Branch History Correlation Table" << std::endl;
    // Display header
    std::cout << "br_pc,count,total_taken";
    for(const auto& branch: sorted_static_branches) {
        std::cout << "," << std::hex << branch.pc;
    }
    std::cout << std::endl;

    // Branch History Correlation table
    for(const auto& branch: sorted_static_branches) {
        std::cout << std::hex << branch.pc
                  << "," << std::dec << branch.count
                  << "," << branch.total_taken;
        const auto itr = hist_corr_matrix.find(branch.pc);
        stf_assert(itr != hist_corr_matrix.end(),"Bad pc=" << std::hex << branch.pc);
        const auto &hist_corr_row = itr->second;
        for(uint32_t idx=0; idx<MAX_HISTORY; ++idx) {
            const HistCorrData &cdata = hist_corr_row.at(idx);
            std::cout << "," << boost::format("%0.3f") % (double(cdata.corr_cnt)/branch.count);
        }
        std::cout << std::endl;
    }
}
