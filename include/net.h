#pragma once
#include <limits>
#include <map>
#include <vector>
#include <unordered_map>
#include <string>


constexpr unsigned INF = 1e9;
constexpr unsigned LARGE = 1e3;
constexpr unsigned NOT_FOUND = std::numeric_limits<unsigned>::max();

struct Node {
    bool _is_large;
    union {
        std::map<unsigned, unsigned>* map_ptr;
        std::vector<std::pair<Node*, unsigned>>* vector_ptr; 
    } data;
    Node();
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&& other);
    Node& operator=(Node&& other);
    unsigned find(unsigned node);
    void fill(Node &from, unsigned dist);
    ~Node();
};

void generate_unordered_temporal_graph(const std::string &filename, unsigned num_v = 1e6,
        unsigned num_e = 1e7, unsigned period = 1e4);

void generate_ordered_temporal_graph(const std::string &filename, unsigned num_v = 1e6,
        unsigned num_e = 1e6, unsigned period = 1e4);

template <typename F, typename... Args>
void calc_time(F&& func, Args&&... args);

void path_ordered_finder(std::vector<unsigned> &path, const std::string &filename, unsigned start_node = 1, unsigned num_v = 1e6,
        unsigned num_e = 1e6, unsigned period = 1e4);

void path_fill(std::vector<std::unordered_map<unsigned, unsigned>> &path, std::vector<unsigned> &node_times,
                const unsigned node_from, const unsigned node_to, const unsigned cur_time);

void path_ordered_finder_adjacency_map(std::vector<std::unordered_map<unsigned, unsigned>> &path, const std::string &filename, const unsigned num_v = 1e6);

void path_fill_node(std::vector<Node> &path, std::vector<unsigned> &node_times,
                const unsigned node_from, const unsigned node_to, const unsigned cur_time);

void path_ordered_finder_node(std::vector<Node> &path, const std::string &filename, const unsigned num_v = 1e6);