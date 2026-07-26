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
#include <utility>
#include <vector>

using temporal_index::HybridTemporalIndex;
using temporal_index::QueryMetrics;
using temporal_index::SyntheticConfig;
using temporal_index::SyntheticModel;
using temporal_index::TemporalEdge;
using temporal_index::Time;
using temporal_index::Vertex;
using temporal_index::generate_synthetic_events;
using temporal_index::parse_synthetic_model;
using temporal_index::synthetic_model_name;

namespace {
struct Config {
    std::size_t vertices{1000};
    std::size_t events{20000};
    std::size_t queries{5000};
    std::size_t verify_queries{100};
    std::size_t profile_queries{100};
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
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return result;
}

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (++i >= argc) {
                throw std::invalid_argument("missing value for " + arg);
            }
            return argv[i];
        };
        if (arg == "--vertices") {
            config.vertices = std::stoull(value());
        } else if (arg == "--events") {
            config.events = std::stoull(value());
        } else if (arg == "--queries") {
            config.queries = std::stoull(value());
        } else if (arg == "--verify-queries") {
            config.verify_queries = std::stoull(value());
        } else if (arg == "--profile-queries") {
            config.profile_queries = std::stoull(value());
        } else if (arg == "--time-buckets") {
            config.time_buckets = std::stoull(value());
        } else if (arg == "--gamma") {
            config.gamma = std::stod(value());
        } else if (arg == "--seed") {
            config.seed = std::stoull(value());
        } else if (arg == "--model") {
            config.model = parse_synthetic_model(value());
        } else if (arg == "--thresholds") {
            config.thresholds = parse_thresholds(value());
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }
    if (config.vertices < 2 || config.events == 0 || config.time_buckets == 0 ||
        config.queries == 0 || config.thresholds.empty()) {
        throw std::invalid_argument("invalid benchmark dimensions");
    }
    if (config.gamma <= 1.0) {
        throw std::invalid_argument("gamma must exceed 1");
    }
    return config;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(
        p * static_cast<double>(values.size() - 1));
    return values[index];
}

double mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) /
           static_cast<double>(values.size());
}

double mean_counter(std::uint64_t total, std::size_t count) {
    if (count == 0) {
        return 0.0;
    }
    return static_cast<double>(total) / static_cast<double>(count);
}
}  // namespace

int main(int argc, char** argv) {
    const Config config = parse_args(argc, argv);
    const SyntheticConfig synthetic_config{
        config.vertices,
        config.events,
        config.time_buckets,
        config.gamma,
        config.seed,
        config.model};
    const auto edges = generate_synthetic_events(synthetic_config);

    std::cout
        << "seed,model,vertices,events,time_buckets,gamma_input,threshold,build_ms,"
           "query_mean_us,query_p50_us,query_p95_us,query_p99_us,"
           "small_target_queries,small_query_mean_us,small_query_p50_us,"
           "small_query_p95_us,small_query_p99_us,"
           "large_target_queries,large_query_mean_us,large_query_p50_us,"
           "large_query_p95_us,large_query_p99_us,"
           "profiled_queries,profiled_large_queries,"
           "large_states_mean,scanned_edges_mean,small_lookups_mean,"
           "successful_small_lookups_mean,"
           "large_vertices,large_fraction,stored_label_entries,"
           "verified_queries,verification_mismatches\n";

    // All thresholds use exactly the same query pairs for one seed.
    std::mt19937_64 workload_rng(
        config.seed + 0x9e3779b97f4a7c15ULL);
    std::uniform_int_distribution<Vertex> vertex_dist(
        0, config.vertices - 1);

    const auto make_query_pairs = [&](std::size_t count) {
        std::vector<std::pair<Vertex, Vertex>> pairs;
        pairs.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            Vertex source = vertex_dist(workload_rng);
            Vertex target = vertex_dist(workload_rng);
            while (target == source) {
                target = vertex_dist(workload_rng);
            }
            pairs.emplace_back(source, target);
        }
        return pairs;
    };

    const auto timed_queries = make_query_pairs(config.queries);
    const auto verification_queries = make_query_pairs(config.verify_queries);
    const auto profiled_query_count =
        std::min(config.profile_queries, timed_queries.size());

    for (const auto threshold : config.thresholds) {
        const auto build_start = std::chrono::steady_clock::now();
        HybridTemporalIndex index(config.vertices, threshold);
        for (const auto& edge : edges) {
            index.add_edge(edge.source, edge.target, edge.time);
        }
        index.finalize();
        const auto build_end = std::chrono::steady_clock::now();

        std::vector<double> query_us;
        std::vector<double> small_query_us;
        std::vector<double> large_query_us;
        query_us.reserve(timed_queries.size());
        small_query_us.reserve(timed_queries.size());
        large_query_us.reserve(timed_queries.size());

        for (const auto& [source, target] : timed_queries) {
            // Classification is outside the timed region.
            const bool target_large = index.is_large(target);
            const auto start = std::chrono::steady_clock::now();
            volatile auto answer = index.earliest_arrival(source, target);
            (void)answer;
            const auto end = std::chrono::steady_clock::now();
            const double elapsed_us =
                std::chrono::duration<double, std::micro>(end - start).count();

            query_us.push_back(elapsed_us);
            if (target_large) {
                large_query_us.push_back(elapsed_us);
            } else {
                small_query_us.push_back(elapsed_us);
            }
        }

        // Traversal counters are collected in a separate, untimed pass.
        std::size_t profiled_large_queries = 0;
        std::uint64_t total_large_states = 0;
        std::uint64_t total_scanned_edges = 0;
        std::uint64_t total_small_lookups = 0;
        std::uint64_t total_successful_small_lookups = 0;

        for (std::size_t i = 0; i < profiled_query_count; ++i) {
            const auto [source, target] = timed_queries[i];
            QueryMetrics metrics;
            volatile auto answer =
                index.earliest_arrival_with_metrics(source, target, metrics);
            (void)answer;

            if (metrics.target_large) {
                ++profiled_large_queries;
                total_large_states += metrics.visited_large_states;
                total_scanned_edges += metrics.scanned_incoming_edges;
                total_small_lookups += metrics.small_label_lookups;
                total_successful_small_lookups +=
                    metrics.successful_small_label_lookups;
            }
        }

        std::size_t mismatches = 0;
        for (const auto& [source, target] : verification_queries) {
            const auto exact = temporal_index::exact_earliest_arrival(
                config.vertices, edges, source, target);
            const auto hybrid = index.earliest_arrival(source, target);
            if (exact != hybrid) {
                ++mismatches;
            }
        }

        const auto large = index.large_vertex_count();
        std::cout
            << config.seed << ',' << synthetic_model_name(config.model) << ','
            << config.vertices << ',' << config.events << ','
            << config.time_buckets << ',' << config.gamma << ',' << threshold << ','
            << std::chrono::duration<double, std::milli>(
                   build_end - build_start).count()
            << ',' << mean(query_us) << ',' << percentile(query_us, 0.50) << ','
            << percentile(query_us, 0.95) << ',' << percentile(query_us, 0.99)
            << ',' << small_query_us.size() << ',' << mean(small_query_us) << ','
            << percentile(small_query_us, 0.50) << ','
            << percentile(small_query_us, 0.95) << ','
            << percentile(small_query_us, 0.99) << ','
            << large_query_us.size() << ',' << mean(large_query_us) << ','
            << percentile(large_query_us, 0.50) << ','
            << percentile(large_query_us, 0.95) << ','
            << percentile(large_query_us, 0.99) << ','
            << profiled_query_count << ',' << profiled_large_queries << ','
            << mean_counter(total_large_states, profiled_large_queries) << ','
            << mean_counter(total_scanned_edges, profiled_large_queries) << ','
            << mean_counter(total_small_lookups, profiled_large_queries) << ','
            << mean_counter(
                   total_successful_small_lookups, profiled_large_queries)
            << ',' << large << ','
            << static_cast<double>(large) /
                   static_cast<double>(config.vertices)
            << ',' << index.stored_label_entries() << ','
            << verification_queries.size() << ',' << mismatches << '\n';
    }
}
