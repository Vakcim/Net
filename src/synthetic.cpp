#include "temporal_index/synthetic.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace temporal_index {

SyntheticModel parse_synthetic_model(const std::string& name) {
    if (name == "dag-pareto") return SyntheticModel::DagPareto;
    if (name == "random-pareto") return SyntheticModel::RandomPareto;
    throw std::invalid_argument("unknown synthetic model: " + name);
}

std::string synthetic_model_name(SyntheticModel model) {
    switch (model) {
        case SyntheticModel::DagPareto: return "dag-pareto";
        case SyntheticModel::RandomPareto: return "random-pareto";
    }
    throw std::logic_error("unhandled synthetic model");
}

std::vector<TemporalEdge> generate_synthetic_events(const SyntheticConfig& config) {
    if (config.vertex_count < 2 || config.event_count == 0 || config.time_buckets == 0) {
        throw std::invalid_argument("invalid synthetic graph dimensions");
    }
    if (config.gamma <= 1.0) {
        throw std::invalid_argument("gamma must exceed 1");
    }

    std::mt19937_64 rng(config.seed);
    std::uniform_real_distribution<double> uniform01(0.0, 1.0);
    std::uniform_int_distribution<Vertex> any_vertex(0, config.vertex_count - 1);

    // Latent target attractiveness has a Pareto tail. The realized indegree
    // exponent is measured after generation; gamma is a generator parameter,
    // not a guaranteed fitted exponent.
    const double shape = config.gamma - 1.0;
    std::vector<double> cumulative(config.vertex_count, 0.0);
    double total = 0.0;
    for (std::size_t v = 0; v < config.vertex_count; ++v) {
        const double u = std::max(uniform01(rng), 1e-12);
        const double weight = std::pow(u, -1.0 / shape);
        total += weight;
        cumulative[v] = total;
    }

    auto weighted_vertex = [&](Vertex lower_bound) -> Vertex {
        if (lower_bound >= config.vertex_count) {
            throw std::invalid_argument("weighted_vertex lower bound is outside graph");
        }
        const double offset = lower_bound == 0 ? 0.0 : cumulative[lower_bound - 1];
        std::uniform_real_distribution<double> draw(offset, total);
        const double x = draw(rng);
        return static_cast<Vertex>(
            std::lower_bound(cumulative.begin() + lower_bound, cumulative.end(), x) -
            cumulative.begin());
    };

    std::vector<TemporalEdge> edges;
    edges.reserve(config.event_count);
    for (std::size_t i = 0; i < config.event_count; ++i) {
        Vertex source{};
        Vertex target{};

        if (config.model == SyntheticModel::DagPareto) {
            // Directed acyclic support prevents almost-sure strong-connectivity
            // saturation. Targets are heavy-tailed; sources are chosen from
            // lower-ranked vertices, so source < target.
            target = weighted_vertex(1);
            std::uniform_int_distribution<Vertex> source_dist(0, target - 1);
            source = source_dist(rng);
        } else {
            source = any_vertex(rng);
            target = weighted_vertex(0);
            while (target == source) target = weighted_vertex(0);
        }

        const Time time = static_cast<Time>((i * config.time_buckets) / config.event_count);
        edges.push_back({source, target, time});
    }
    return edges;
}

}  // namespace temporal_index
