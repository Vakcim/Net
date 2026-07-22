#include "temporal_index/temporal_index.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using temporal_index::HybridTemporalIndex;
using temporal_index::TemporalEdge;
using temporal_index::Vertex;

namespace {
void usage(const char* program) {
    std::cerr << "Usage: " << program
              << " --vertices N --threshold B --input edges.csv [--source S --target T]\n"
              << "CSV format: source,target,time (header is optional).\n";
}

std::vector<TemporalEdge> read_edges(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open input file: " + path);

    std::vector<TemporalEdge> edges;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line[0] == '#') continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream parser(line);
        TemporalEdge edge;
        if (!(parser >> edge.source >> edge.target >> edge.time)) {
            if (line_number == 1) continue;  // optional header
            throw std::runtime_error("invalid CSV row at line " + std::to_string(line_number));
        }
        edges.push_back(edge);
    }
    return edges;
}
}  // namespace

int main(int argc, char** argv) {
    std::size_t vertices = 0;
    std::size_t threshold = 0;
    std::string input_path;
    std::optional<Vertex> source;
    std::optional<Vertex> target;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (++i >= argc) throw std::invalid_argument("missing value for " + name);
            return argv[i];
        };
        if (arg == "--vertices") vertices = std::stoull(require_value(arg));
        else if (arg == "--threshold") threshold = std::stoull(require_value(arg));
        else if (arg == "--input") input_path = require_value(arg);
        else if (arg == "--source") source = static_cast<Vertex>(std::stoul(require_value(arg)));
        else if (arg == "--target") target = static_cast<Vertex>(std::stoul(require_value(arg)));
        else if (arg == "--help" || arg == "-h") { usage(argv[0]); return 0; }
        else throw std::invalid_argument("unknown argument: " + arg);
    }

    if (vertices == 0 || threshold == 0 || input_path.empty()) {
        usage(argv[0]);
        return 2;
    }
    if (source.has_value() != target.has_value()) {
        throw std::invalid_argument("--source and --target must be supplied together");
    }

    const auto edges = read_edges(input_path);
    HybridTemporalIndex index(vertices, threshold);
    for (const auto& edge : edges) index.add_edge(edge.source, edge.target, edge.time);
    index.finalize();

    std::cout << "vertices=" << index.vertex_count() << '\n'
              << "edges=" << index.edge_count() << '\n'
              << "threshold=" << index.promotion_threshold() << '\n'
              << "large_vertices=" << index.large_vertex_count() << '\n'
              << "stored_label_entries=" << index.stored_label_entries() << '\n';

    if (source) {
        const auto answer = index.earliest_arrival(*source, *target);
        std::cout << "earliest_arrival=";
        if (answer) std::cout << *answer << '\n';
        else std::cout << "unreachable\n";
    }
    return 0;
}
