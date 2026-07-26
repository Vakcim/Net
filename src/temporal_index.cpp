#include "temporal_index/temporal_index.hpp"

#include <algorithm>
#include <unordered_set>

namespace temporal_index {

namespace {
constexpr Time kSourceArrival = 0;
}

HybridTemporalIndex::HybridTemporalIndex(
    std::size_t vertex_count,
    std::size_t promotion_threshold)
    : nodes_(vertex_count), threshold_(promotion_threshold) {
    if (vertex_count == 0) {
        throw std::invalid_argument("vertex_count must be positive");
    }
    if (promotion_threshold == 0) {
        throw std::invalid_argument("promotion_threshold must be positive");
    }
}

std::size_t HybridTemporalIndex::QueryStateHash::operator()(
    const QueryState& state) const noexcept {
    const auto h1 = std::hash<Vertex>{}(state.vertex);
    const auto h2 = std::hash<Time>{}(state.deadline);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6U) + (h1 >> 2U));
}

void HybridTemporalIndex::check_vertex(Vertex vertex) const {
    if (vertex >= nodes_.size()) {
        throw std::out_of_range("vertex id is outside [0, vertex_count)");
    }
}

void HybridTemporalIndex::add_edge(Vertex source, Vertex target, Time time) {
    check_vertex(source);
    check_vertex(target);

    if (pending_time_.has_value() && time < *pending_time_) {
        throw std::invalid_argument("temporal edges must be added in nondecreasing time order");
    }

    if (pending_time_.has_value() && time > *pending_time_) {
        process_pending_batch();
    }

    if (!pending_time_.has_value()) {
        pending_time_ = time;
    }

    pending_batch_.push_back({source, target, time});
    finalized_ = false;
    ++edge_count_;
}

void HybridTemporalIndex::finalize() {
    process_pending_batch();
    finalized_ = true;
}

void HybridTemporalIndex::promote(Vertex vertex) {
    auto& node = nodes_[vertex];
    if (node.large) {
        return;
    }
    node.large = true;
    node.labels.clear();
    node.labels.rehash(0);
}

void HybridTemporalIndex::process_pending_batch() {
    if (!pending_time_.has_value()) {
        return;
    }

    // All decisions in this loop use the graph state strictly before the
    // current timestamp. We commit labels and promotions only after every edge
    // in the batch has been inspected. Therefore an edge at time t cannot use
    // reachability that was itself created at time t.
    std::unordered_map<Vertex, std::unordered_map<Vertex, Time>> additions;
    std::unordered_set<Vertex> forced_promotions;

    for (const auto& edge : pending_batch_) {
        nodes_[edge.target].incoming.push_back({edge.source, edge.time});

        if (edge.source == edge.target) {
            continue;
        }

        const auto& source_node = nodes_[edge.source];
        const auto& target_node = nodes_[edge.target];

        if (target_node.large) {
            continue;
        }

        // The source was already large before time t. Hence it had at least B
        // predecessors whose arrivals are strictly earlier than t, so all of
        // them can traverse the current edge and the target cannot remain small.
        if (source_node.large) {
            forced_promotions.insert(edge.target);
            continue;
        }

        auto& target_additions = additions[edge.target];
        target_additions.emplace(edge.source, edge.time);  // direct one-edge path

        // source_node.labels contains only arrivals from earlier timestamp
        // batches, hence every stored arrival is strictly smaller than edge.time.
        for (const auto& [predecessor, arrival_at_source] : source_node.labels) {
            if (arrival_at_source < edge.time && predecessor != edge.target) {
                target_additions.emplace(predecessor, edge.time);
            }
        }
    }

    // A forced promotion dominates label insertion for the same target.
    for (const Vertex target : forced_promotions) {
        promote(target);
    }

    for (auto& [target, target_additions] : additions) {
        auto& target_node = nodes_[target];
        if (target_node.large) {
            continue;
        }

        for (const auto& [source, arrival] : target_additions) {
            if (source == target) {
                continue;
            }
            auto [it, inserted] = target_node.labels.emplace(source, arrival);
            if (!inserted && arrival < it->second) {
                it->second = arrival;
            }
        }

        if (target_node.labels.size() >= threshold_) {
            promote(target);
        }
    }

    pending_batch_.clear();
    pending_time_.reset();
}

bool HybridTemporalIndex::reachable_before(
    Vertex source,
    Vertex target,
    Time deadline,
    std::unordered_map<QueryState, bool, QueryStateHash>& memo,
    std::unordered_map<QueryState, bool, QueryStateHash>& active,
    QueryMetrics* metrics) const {
    if (source == target) {
        return true;
    }

    const QueryState state{target, deadline};
    if (const auto it = memo.find(state); it != memo.end()) {
        return it->second;
    }
    if (active.contains(state)) {
        return false;
    }
    active.emplace(state, true);

    const auto& node = nodes_[target];
    bool answer = false;

    if (!node.large) {
        if (metrics != nullptr) {
            ++metrics->small_label_lookups;
        }
        const auto it = node.labels.find(source);
        answer = it != node.labels.end() && it->second < deadline;
        if (answer && metrics != nullptr) {
            ++metrics->successful_small_label_lookups;
        }
    } else {
        if (metrics != nullptr) {
            ++metrics->visited_large_states;
        }
        for (const auto& edge : node.incoming) {
            if (edge.time >= deadline) {
                break;
            }
            if (metrics != nullptr) {
                ++metrics->scanned_incoming_edges;
            }
            if (edge.source == source ||
                reachable_before(
                    source, edge.source, edge.time, memo, active, metrics)) {
                answer = true;
                break;
            }
        }
    }

    active.erase(state);
    memo.emplace(state, answer);
    return answer;
}

std::optional<Time> HybridTemporalIndex::earliest_arrival(
    Vertex source,
    Vertex target) const {
    return earliest_arrival_impl(source, target, nullptr);
}

std::optional<Time> HybridTemporalIndex::earliest_arrival_with_metrics(
    Vertex source,
    Vertex target,
    QueryMetrics& metrics) const {
    metrics = QueryMetrics{};
    return earliest_arrival_impl(source, target, &metrics);
}

std::optional<Time> HybridTemporalIndex::earliest_arrival_impl(
    Vertex source,
    Vertex target,
    QueryMetrics* metrics) const {
    check_vertex(source);
    check_vertex(target);
    if (!finalized_ || pending_time_.has_value()) {
        throw std::logic_error("finalize() must be called before querying the index");
    }
    if (source == target) {
        return kSourceArrival;
    }

    const auto& target_node = nodes_[target];
    if (metrics != nullptr) {
        metrics->target_large = target_node.large;
    }

    if (!target_node.large) {
        if (metrics != nullptr) {
            ++metrics->small_label_lookups;
        }
        const auto it = target_node.labels.find(source);
        if (it == target_node.labels.end()) {
            return std::nullopt;
        }
        if (metrics != nullptr) {
            ++metrics->successful_small_label_lookups;
        }
        return it->second;
    }

    if (metrics != nullptr) {
        ++metrics->visited_large_states;
    }

    std::unordered_map<QueryState, bool, QueryStateHash> memo;
    std::unordered_map<QueryState, bool, QueryStateHash> active;

    // Incoming edges are appended in nondecreasing timestamp order. The first
    // feasible last edge therefore gives the earliest arrival at target.
    for (const auto& edge : target_node.incoming) {
        if (metrics != nullptr) {
            ++metrics->scanned_incoming_edges;
        }
        if (edge.source == source ||
            reachable_before(
                source, edge.source, edge.time, memo, active, metrics)) {
            return edge.time;
        }
    }
    return std::nullopt;
}

std::size_t HybridTemporalIndex::large_vertex_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        nodes_.begin(), nodes_.end(), [](const Node& node) { return node.large; }));
}

std::size_t HybridTemporalIndex::stored_label_entries() const noexcept {
    std::size_t total = 0;
    for (const auto& node : nodes_) {
        total += node.labels.size();
    }
    return total;
}

bool HybridTemporalIndex::is_large(Vertex vertex) const {
    check_vertex(vertex);
    return nodes_[vertex].large;
}

std::optional<std::size_t> HybridTemporalIndex::small_predecessor_count(Vertex vertex) const {
    check_vertex(vertex);
    if (nodes_[vertex].large) {
        return std::nullopt;
    }
    return nodes_[vertex].labels.size();
}

std::optional<Time> exact_earliest_arrival(
    std::size_t vertex_count,
    const std::vector<TemporalEdge>& sorted_edges,
    Vertex source,
    Vertex target) {
    if (source >= vertex_count || target >= vertex_count) {
        throw std::out_of_range("vertex id is outside [0, vertex_count)");
    }
    if (source == target) {
        return kSourceArrival;
    }

    for (std::size_t i = 1; i < sorted_edges.size(); ++i) {
        if (sorted_edges[i].time < sorted_edges[i - 1].time) {
            throw std::invalid_argument("reference scan requires nondecreasing timestamps");
        }
    }

    std::vector<std::optional<Time>> arrival(vertex_count);

    for (const auto& edge : sorted_edges) {
        const bool source_reachable =
            edge.source == source ||
            (arrival[edge.source].has_value() && *arrival[edge.source] < edge.time);

        if (source_reachable &&
            (!arrival[edge.target].has_value() || edge.time < *arrival[edge.target])) {
            arrival[edge.target] = edge.time;
        }
    }

    return arrival[target];
}

}  // namespace temporal_index
