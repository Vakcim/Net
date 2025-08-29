#pragma once
#include <limits>
#include <map>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <iostream>


constexpr uint32_t INF = 1e9;
constexpr uint32_t LARGE = 1e3;
constexpr uint32_t NOT_FOUND = std::numeric_limits<uint32_t>::max();

struct Node {
    bool _is_large;
    union {
        std::unordered_map<uint32_t, uint32_t>* map_ptr;
        std::vector<std::pair<Node*, uint32_t>>* vector_ptr; 
    } data;
    Node();
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&& other);
    Node& operator=(Node&& other);
    uint32_t find(uint32_t node);
    void fill(Node &from, uint32_t dist);
    ~Node();
};

void generate_unordered_temporal_graph(const std::string &filename, uint32_t num_v = 1e6,
        uint32_t num_e = 1e7, uint32_t period = 1e4);

void generate_ordered_temporal_graph(const std::string &filename, uint32_t num_v = 1e6,
        uint32_t num_e = 1e6, uint32_t period = 1e4);

template <typename F, typename... Args>
void calc_time(F &&func, Args &&...args) {
  auto start = std::chrono::high_resolution_clock::now();
  func(std::forward<Args>(args)...);
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "function woring for: " << duration.count() / 1000000 << '.'
            << duration.count() % 1000000 << " seconds ";
}

void path_ordered_finder(std::vector<uint32_t> &path, const std::string &filename, uint32_t start_node = 1, uint32_t num_v = 1e6,
        uint32_t num_e = 1e6, uint32_t period = 1e4);

void path_fill(std::vector<std::unordered_map<uint32_t, uint32_t>> &path, std::vector<uint32_t> &node_times,
                const uint32_t node_from, const uint32_t node_to, const uint32_t cur_time);

void path_ordered_finder_adjacency_map(std::vector<std::unordered_map<uint32_t, uint32_t>> &path, const std::string &filename, const uint32_t num_v = 1e6);

void path_fill_node(std::vector<Node> &path, std::vector<uint32_t> &node_times,
                const uint32_t node_from, const uint32_t node_to, const uint32_t cur_time);

void path_ordered_finder_node(std::vector<Node> &path, const std::string &filename, const uint32_t num_v = 1e6);

void print_memory_usage();