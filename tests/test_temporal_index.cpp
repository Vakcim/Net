#include "temporal_index/temporal_index.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using temporal_index::HybridTemporalIndex;
using temporal_index::TemporalEdge;
using temporal_index::Time;
using temporal_index::Vertex;
using temporal_index::exact_earliest_arrival;

namespace {
int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                      \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__      \
                      << ": " #condition "\n";                                  \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

void check_equal(const std::optional<Time>& actual,
                 const std::optional<Time>& expected,
                 const std::string& context) {
    if (actual != expected) {
        std::cerr << "Mismatch: " << context << ", actual=";
        if (actual) std::cerr << *actual; else std::cerr << "none";
        std::cerr << ", expected=";
        if (expected) std::cerr << *expected; else std::cerr << "none";
        std::cerr << '\n';
        ++failures;
    }
}

HybridTemporalIndex build(std::size_t n,
                          std::size_t threshold,
                          const std::vector<TemporalEdge>& edges) {
    auto sorted = edges;
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.time < b.time;
    });
    HybridTemporalIndex index(n, threshold);
    for (const auto& edge : sorted) {
        index.add_edge(edge.source, edge.target, edge.time);
    }
    index.finalize();
    return index;
}

void test_strict_chain() {
    const std::vector<TemporalEdge> edges{{0, 1, 1}, {1, 2, 3}, {2, 3, 8}};
    auto index = build(4, 10, edges);
    check_equal(index.earliest_arrival(0, 3), 8, "strict chain");
    check_equal(index.earliest_arrival(1, 3), 8, "chain suffix");
    check_equal(index.earliest_arrival(3, 0), std::nullopt, "unreachable reverse");
}

void test_equal_times_do_not_chain() {
    const std::vector<TemporalEdge> edges{{1, 2, 5}, {0, 1, 5}};
    auto index = build(3, 10, edges);
    check_equal(index.earliest_arrival(0, 1), 5, "direct equal-time edge");
    check_equal(index.earliest_arrival(0, 2), std::nullopt,
                "strict semantics reject equal-time chaining");
}

void test_equal_time_order_independence() {
    const std::vector<TemporalEdge> first{{0, 1, 5}, {1, 2, 5}};
    const std::vector<TemporalEdge> second{{1, 2, 5}, {0, 1, 5}};
    auto index_a = build(3, 10, first);
    auto index_b = build(3, 10, second);
    check_equal(index_a.earliest_arrival(0, 2), std::nullopt,
                "equal-time order A");
    check_equal(index_b.earliest_arrival(0, 2), std::nullopt,
                "equal-time order B");
}

void test_large_predecessor_promotes_target_at_later_time() {
    const std::vector<TemporalEdge> edges{
        {0, 2, 1}, {1, 2, 2},  // vertex 2 becomes large for B=2
        {2, 3, 3}
    };
    auto index = build(4, 2, edges);
    CHECK(index.is_large(2));
    CHECK(index.is_large(3));
    check_equal(index.earliest_arrival(0, 3), 3,
                "strict path through previously large predecessor");
}

void test_same_batch_promotion_does_not_propagate() {
    // Vertex 2 is promoted at time 5, but its newly discovered predecessors at
    // time 5 cannot traverse 2 -> 3 at the same time under strict semantics.
    const std::vector<TemporalEdge> edges{{0, 2, 5}, {1, 2, 5}, {2, 3, 5}};
    auto index = build(4, 2, edges);
    CHECK(index.is_large(2));
    CHECK(!index.is_large(3));
    check_equal(index.earliest_arrival(0, 3), std::nullopt,
                "same-batch promotion must not create a strict path");
    check_equal(index.earliest_arrival(2, 3), 5,
                "direct edge from promoted source remains valid");
}

void test_earliest_of_multiple_paths() {
    const std::vector<TemporalEdge> edges{
        {0, 1, 1}, {1, 3, 9},
        {0, 2, 2}, {2, 3, 4}
    };
    auto index = build(4, 1, edges);  // force traversal-heavy queries
    check_equal(index.earliest_arrival(0, 3), 4, "minimum arrival over paths");
}

void test_equal_time_cycle_does_not_extend_path() {
    const std::vector<TemporalEdge> edges{{0, 1, 7}, {1, 2, 7}, {2, 1, 7}, {2, 3, 7}};
    auto index = build(4, 2, edges);
    check_equal(index.earliest_arrival(0, 1), 7, "direct edge into equal-time cycle");
    check_equal(index.earliest_arrival(0, 3), std::nullopt,
                "equal-time cycle cannot be traversed strictly");
}

void test_zero_timestamp_direct_edge() {
    const std::vector<TemporalEdge> edges{{0, 1, 0}, {1, 2, 1}};
    auto index = build(3, 10, edges);
    check_equal(index.earliest_arrival(0, 1), 0, "direct edge at timestamp zero");
    check_equal(index.earliest_arrival(0, 2), 1, "strict continuation after zero");
}

void test_order_validation() {
    HybridTemporalIndex index(3, 2);
    index.add_edge(0, 1, 10);
    bool threw = false;
    try {
        index.add_edge(1, 2, 9);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void test_query_requires_finalize() {
    HybridTemporalIndex index(2, 2);
    index.add_edge(0, 1, 1);
    bool threw = false;
    try {
        (void)index.earliest_arrival(0, 1);
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}

void fuzz_against_reference() {
    constexpr std::size_t n = 8;
    constexpr std::size_t m = 28;
    const std::vector<std::size_t> thresholds{1, 2, 3, 5, 9};

    for (std::uint64_t seed = 0; seed < 150; ++seed) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<Vertex> vertex_dist(0, n - 1);
        // Deliberately few timestamp values to create many equal-time edges.
        std::uniform_int_distribution<Time> time_dist(0, 8);

        std::vector<TemporalEdge> edges;
        edges.reserve(m);
        for (std::size_t i = 0; i < m; ++i) {
            Vertex u = vertex_dist(rng);
            Vertex v = vertex_dist(rng);
            while (v == u) v = vertex_dist(rng);
            edges.push_back({u, v, time_dist(rng)});
        }
        std::stable_sort(edges.begin(), edges.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });

        for (const auto threshold : thresholds) {
            auto index = build(n, threshold, edges);
            for (Vertex source = 0; source < n; ++source) {
                for (Vertex target = 0; target < n; ++target) {
                    const auto expected = exact_earliest_arrival(n, edges, source, target);
                    const auto actual = index.earliest_arrival(source, target);
                    if (actual != expected) {
                        std::ostringstream context;
                        context << "seed=" << seed << ", B=" << threshold
                                << ", source=" << source << ", target=" << target;
                        check_equal(actual, expected, context.str());
                        return;
                    }
                }
            }
        }
    }
}

}  // namespace

int main() {
    test_strict_chain();
    test_equal_times_do_not_chain();
    test_equal_time_order_independence();
    test_large_predecessor_promotes_target_at_later_time();
    test_same_batch_promotion_does_not_propagate();
    test_earliest_of_multiple_paths();
    test_equal_time_cycle_does_not_extend_path();
    test_zero_timestamp_direct_edge();
    test_order_validation();
    test_query_requires_finalize();
    fuzz_against_reference();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All strict temporal-index tests passed.\n";
    return EXIT_SUCCESS;
}
