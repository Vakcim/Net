#include "temporal_index/synthetic.hpp"
#include "temporal_index/temporal_index.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using temporal_index::HybridTemporalIndex;
using temporal_index::SyntheticConfig;
using temporal_index::SyntheticModel;
using temporal_index::Vertex;
using temporal_index::generate_synthetic_events;
using temporal_index::parse_synthetic_model;
using temporal_index::synthetic_model_name;

namespace {
struct Config {
    std::size_t vertices{1000};
    std::size_t events{10000};
    std::size_t time_buckets{1000};
    double gamma{2.5};
    std::uint64_t seed{42};
    SyntheticModel model{SyntheticModel::DagPareto};
};

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (++i >= argc) throw std::invalid_argument("missing value for " + arg);
            return argv[i];
        };
        if (arg == "--vertices") config.vertices = std::stoull(value());
        else if (arg == "--events") config.events = std::stoull(value());
        else if (arg == "--time-buckets") config.time_buckets = std::stoull(value());
        else if (arg == "--gamma") config.gamma = std::stod(value());
        else if (arg == "--seed") config.seed = std::stoull(value());
        else if (arg == "--model") config.model = parse_synthetic_model(value());
        else throw std::invalid_argument("unknown argument: " + arg);
    }
    return config;
}
}  // namespace

int main(int argc, char** argv) {
    const Config config = parse_args(argc, argv);
    const SyntheticConfig generator{
        config.vertices, config.events, config.time_buckets, config.gamma, config.seed, config.model};
    const auto edges = generate_synthetic_events(generator);

    std::vector<std::size_t> indegree(config.vertices, 0);
    for (const auto& edge : edges) ++indegree[edge.target];

    // Maximum predecessor count excludes self, so threshold n prevents every
    // promotion and exposes exact R_v for all vertices.
    HybridTemporalIndex full_index(config.vertices, config.vertices);
    for (const auto& edge : edges) full_index.add_edge(edge.source, edge.target, edge.time);
    full_index.finalize();

    std::cout << "seed,model,gamma_input,vertex,indegree,predecessor_count\n";
    for (Vertex vertex = 0; vertex < config.vertices; ++vertex) {
        const auto count = full_index.small_predecessor_count(vertex);
        if (!count) throw std::logic_error("full profile index unexpectedly promoted a vertex");
        std::cout << config.seed << ',' << synthetic_model_name(config.model) << ','
                  << config.gamma << ',' << vertex << ',' << indegree[vertex] << ','
                  << *count << '\n';
    }
}
