#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>

#include "print_utils.hpp"
#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"

class Node {
    private:
        const uint64_t id_;
        const stf::STFBranch branch_;
        std::set<uint64_t> targets_;
        uint64_t taken_count_ = 0;
        uint64_t not_taken_count_ = 0;

    public:
        Node(const uint64_t id, const stf::STFBranch branch) :
            id_(id),
            branch_(branch)
        {
        }

        void addTarget(const uint64_t target) {
            targets_.emplace(target);
        }

        void countTaken(const bool taken) {
            taken_count_ += taken;
            not_taken_count_ += !taken;
        }

        friend std::ostream& operator<<(std::ostream& os, const Node& node) {
            const auto& branch = node.branch_;
            const bool is_indirect = branch.isIndirect();

            os << "NODE " << node.id_
               << " 0x" << std::hex << branch.getPC()
               << " -"
               << " 0x" << branch.getOpcode()
               << ' ' << std::dec << (branch.isCompressed() ? 2 : 4)
               << " class: " << (branch.isCall() ? "CALL" : (branch.isReturn() ? "RET" : "JMP")) << '+'
                             << (is_indirect ? "IND" : "DIR") << '+'
                             << (branch.isConditional() ? "CND" : "UCD")
               << " behavior: " << (node.taken_count_ == 0 ? "ANT" : (node.not_taken_count_ == 0 ? "AT" : "DYN")) << '+'
                                << (is_indirect ? "IND" : "DIR")
               << " taken_cnt: " << node.taken_count_
               << " not_taken_cnt: " << node.not_taken_count_
               << " tgt_cnt: " << node.targets_.size();
            return os;
        }
};

class Edge {
    private:
        const uint64_t id_;
        const uint64_t src_id_;
        const uint64_t dest_id_;
        const uint64_t target_;
        const uint64_t inst_count_;
        const bool is_taken_;
        uint64_t traverse_count_ = 0;

    public:
        Edge(const uint64_t id,
             const uint64_t src_id,
             const uint64_t dest_id,
             const uint64_t target,
             const uint64_t inst_count,
             const bool is_taken) :
            id_(id),
            src_id_(src_id),
            dest_id_(dest_id),
            target_(target),
            inst_count_(inst_count),
            is_taken_(is_taken)
        {
        }

        void incrementTraverse() {
            ++traverse_count_;
        }

        friend std::ostream& operator<<(std::ostream& os, const Edge& edge) {
            os << "EDGE " << edge.id_
               << ' ' << edge.src_id_
               << ' ' << edge.dest_id_
               << ' ' << (edge.is_taken_ ? 'T' : 'N')
               << " 0x" << std::hex << edge.target_
               << ' ' << std::dec << edge.inst_count_
               << " traverse_cnt: " << edge.traverse_count_;
            return os;
        }
};

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        std::string& output,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_bt9");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.addPositionalArgument("output", "BT9 output filename");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, trace);
    parser.getPositionalArgument(1, output);
}

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

    stf::STFBranchReader reader(trace, skip_non_user);
    std::ofstream outfile(output);

    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    uint64_t next_node_id = 1;
    uint64_t next_edge_id = 1;

    std::map<uint64_t, uint64_t> node_ids;
    std::map<uint64_t, Node> nodes;
    std::map<uint64_t, std::map<uint64_t, uint64_t>> edge_ids;
    std::map<uint64_t, Edge> edges;

    std::vector<uint64_t> edge_order;

    uint64_t last_node_id = 0;
    uint64_t last_inst_index = 0;

    for(const auto& branch: reader) {
        const auto pc = branch.getPC();
        const auto [node_id_it, node_id_inserted] = node_ids.try_emplace(pc, next_node_id);

        const auto node_id = node_id_it->second;

        const auto [node_it, node_inserted] = nodes.try_emplace(node_id, node_id, branch);
        const auto target = branch.getTargetPC();
        const bool taken = branch.isTaken();

        node_it->second.addTarget(target);
        node_it->second.countTaken(taken);

        next_node_id += node_inserted;

        const auto [edge_id_it, edge_inserted] = edge_ids[last_node_id].try_emplace(node_id, next_edge_id);
        const auto edge_id = edge_id_it->second;

        const auto inst_index = branch.instIndex();

        next_edge_id += edge_inserted;
        const auto [edge_it, _] = edges.try_emplace(edge_id,
                                                    edge_id,
                                                    last_node_id,
                                                    node_id,
                                                    target,
                                                    inst_index - last_inst_index,
                                                    taken);
        edge_it->second.incrementTraverse();

        edge_order.emplace_back(edge_id);

        last_inst_index = inst_index;

        last_node_id = node_id;
    }

    outfile << "BT9_SPA_TRACE_FORMAT" << std::endl
            << "# Generated by stf_bt9" << std::endl
            << "bt9_minor_version: 0" << std::endl
            << "has_physical_address: 0" << std::endl
            << "conversion_date: " << std::ctime(&now) << std::endl
            << "original_stf_input_file: " << trace << std::endl
            << "BT9_NODES" << std::endl;

    for(const auto& node: nodes) {
        outfile << node.second << std::endl;
    }

    outfile << "BT9_EDGES" << std::endl;

    for(const auto& edge: edges) {
        outfile << edge.second << std::endl;
    }

    outfile << "BT9_EDGE_SEQUENCE" << std::endl;

    for(const auto& edge: edge_order) {
        outfile << edge << std::endl;
    }

    return 0;
}
