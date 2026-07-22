#include "temporal_index/synthetic.hpp"
#include "temporal_index/temporal_index.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <vector>

using temporal_index::HybridTemporalIndex;
using temporal_index::TemporalEdge;
using temporal_index::Time;
using temporal_index::Vertex;
using temporal_index::SyntheticConfig;
using temporal_index::SyntheticModel;
using temporal_index::generate_synthetic_events;
using temporal_index::parse_synthetic_model;
using temporal_index::synthetic_model_name;

namespace {
struct Config {
    std::size_t vertices{1000};
    std::size_t events{20000};
    std::size_t queries{5000};
    std::size_t verify_queries{100};
    std::size_t time_buckets{2000};
    double gamma{2.5};
    std::uint64_t seed{42};
    SyntheticModel model{SyntheticModel::DagPareto};
    std::vector<std::size_t> thresholds{4, 8, 16, 32, 64, 128, 256};
};

std::vector<std::size_t> parse_thresholds(const std::string& text) {
    std::vector<std::size_t> result;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto comma = text.find(',', start);
        result.push_back(std::stoull(text.substr(start, comma - start)));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return result;
}

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
        else if (arg == "--queries") config.queries = std::stoull(value());
        else if (arg == "--verify-queries") config.verify_queries = std::stoull(value());
        else if (arg == "--time-buckets") config.time_buckets = std::stoull(value());
        else if (arg == "--gamma") config.gamma = std::stod(value());
        else if (arg == "--seed") config.seed = std::stoull(value());
        else if (arg == "--model") config.model = parse_synthetic_model(value());
        else if (arg == "--thresholds") config.thresholds = parse_thresholds(value());
        else throw std::invalid_argument("unknown argument: " + arg);
    }
    if (config.vertices < 2 || config.events == 0 || config.time_buckets == 0) {
        throw std::invalid_argument("invalid benchmark dimensions");
    }
    if (config.gamma <= 1.0) throw std::invalid_argument("gamma must exceed 1");
    return config;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    return values[index];
}
}  // namespace

int main(int argc, char** argv) {
    const Config config = parse_args(argc, argv);
    const SyntheticConfig synthetic_config{
        config.vertices, config.events, config.time_buckets, config.gamma, config.seed, config.model};
    const auto edges = generate_synthetic_events(synthetic_config);

    std::cout << "seed,model,vertices,events,time_buckets,gamma_input,threshold,build_ms,"
                 "query_mean_us,query_p50_us,query_p95_us,query_p99_us,large_vertices,"
                 "large_fraction,stored_label_entries,verified_queries,verification_mismatches\n";

    for (const auto threshold : config.thresholds) {
        const auto build_start = std::chrono::steady_clock::now();
        HybridTemporalIndex index(config.vertices, threshold);
        for (const auto& edge : edges) index.add_edge(edge.source, edge.target, edge.time);
        index.finalize();
        const auto build_end = std::chrono::steady_clock::now();

        std::mt19937_64 query_rng(config.seed + threshold * 7919ULL);
        std::uniform_int_distribution<Vertex> vertex_dist(0, config.vertices - 1);
        std::vector<double> query_us;
        query_us.reserve(config.queries);
        for (std::size_t i = 0; i < config.queries; ++i) {
            Vertex source = vertex_dist(query_rng);
            Vertex target = vertex_dist(query_rng);
            while (target == source) target = vertex_dist(query_rng);
            const auto start = std::chrono::steady_clock::now();
            volatile auto answer = index.earliest_arrival(source, target);
            (void)answer;
            const auto end = std::chrono::steady_clock::now();
            query_us.push_back(
                std::chrono::duration<double, std::micro>(end - start).count());
        }

        std::size_t mismatches = 0;
        std::mt19937_64 verify_rng(config.seed + threshold * 104729ULL);
        for (std::size_t i = 0; i < config.verify_queries; ++i) {
            Vertex source = vertex_dist(verify_rng);
            Vertex target = vertex_dist(verify_rng);
            while (target == source) target = vertex_dist(verify_rng);
            const auto exact = temporal_index::exact_earliest_arrival(
                config.vertices, edges, source, target);
            const auto hybrid = index.earliest_arrival(source, target);
            if (exact != hybrid) ++mismatches;
        }

        const double mean = std::accumulate(query_us.begin(), query_us.end(), 0.0) /
                            static_cast<double>(query_us.size());
        const auto large = index.large_vertex_count();
        std::cout << config.seed << ',' << synthetic_model_name(config.model) << ',' << config.vertices << ',' << config.events << ','
                  << config.time_buckets << ',' << config.gamma << ',' << threshold << ','
                  << std::chrono::duration<double, std::milli>(build_end - build_start).count() << ','
                  << mean << ',' << percentile(query_us, 0.50) << ','
                  << percentile(query_us, 0.95) << ',' << percentile(query_us, 0.99) << ','
                  << large << ',' << static_cast<double>(large) / config.vertices << ','
                  << index.stored_label_entries() << ',' << config.verify_queries << ','
                  << mismatches << '\n';
    }
}
