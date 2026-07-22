#pragma once

#include "temporal_index/temporal_index.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace temporal_index {

enum class SyntheticModel {
    DagPareto,
    RandomPareto,
};

struct SyntheticConfig {
    std::size_t vertex_count{1000};
    std::size_t event_count{10000};
    std::size_t time_buckets{1000};
    double gamma{2.5};
    std::uint64_t seed{42};
    SyntheticModel model{SyntheticModel::DagPareto};
};

SyntheticModel parse_synthetic_model(const std::string& name);
std::string synthetic_model_name(SyntheticModel model);
std::vector<TemporalEdge> generate_synthetic_events(const SyntheticConfig& config);

}  // namespace temporal_index
