#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace temporal_index {

using Vertex = std::uint32_t;
using Time = std::uint64_t;

struct TemporalEdge {
    Vertex source{};
    Vertex target{};
    Time time{};
};

// Exact hybrid index for strict temporal paths:
// t_1 < t_2 < ... < t_k.
//
// Edges must be pushed in nondecreasing timestamp order. Edges sharing one
// timestamp are processed as a simultaneous batch: labels created at time t
// cannot be used by another edge with the same timestamp.
class HybridTemporalIndex {
public:
    HybridTemporalIndex(std::size_t vertex_count, std::size_t promotion_threshold);

    void add_edge(Vertex source, Vertex target, Time time);
    void finalize();

    [[nodiscard]] std::optional<Time> earliest_arrival(Vertex source, Vertex target) const;

    [[nodiscard]] std::size_t vertex_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t promotion_threshold() const noexcept { return threshold_; }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }
    [[nodiscard]] std::size_t large_vertex_count() const noexcept;
    [[nodiscard]] std::size_t stored_label_entries() const noexcept;
    [[nodiscard]] bool is_large(Vertex vertex) const;
    [[nodiscard]] std::optional<std::size_t> small_predecessor_count(Vertex vertex) const;

private:
    struct IncomingEdge {
        Vertex source{};
        Time time{};
    };

    struct Node {
        bool large{false};
        // source -> earliest arrival time. Self is excluded.
        std::unordered_map<Vertex, Time> labels;
        std::vector<IncomingEdge> incoming;
    };

    struct QueryState {
        Vertex vertex{};
        Time deadline{};

        bool operator==(const QueryState& other) const noexcept {
            return vertex == other.vertex && deadline == other.deadline;
        }
    };

    struct QueryStateHash {
        std::size_t operator()(const QueryState& state) const noexcept;
    };

    void check_vertex(Vertex vertex) const;
    void process_pending_batch();
    void promote(Vertex vertex);

    // Returns whether source can reach target with the last edge time strictly
    // smaller than deadline.
    [[nodiscard]] bool reachable_before(
        Vertex source,
        Vertex target,
        Time deadline,
        std::unordered_map<QueryState, bool, QueryStateHash>& memo,
        std::unordered_map<QueryState, bool, QueryStateHash>& active) const;

    std::vector<Node> nodes_;
    std::size_t threshold_{};
    std::size_t edge_count_{};
    std::optional<Time> pending_time_;
    std::vector<TemporalEdge> pending_batch_;
    bool finalized_{true};
};

// Exact reference implementation used by tests and validation.
// Complexity is intentionally high; it is not a benchmark competitor.
std::optional<Time> exact_earliest_arrival(
    std::size_t vertex_count,
    const std::vector<TemporalEdge>& sorted_edges,
    Vertex source,
    Vertex target);

}  // namespace temporal_index
