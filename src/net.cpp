#include "net.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <sstream>
#include <thread>

Node::Node() : _is_large(false) { data.map_ptr = nullptr; data.vector_ptr = nullptr;}

Node::Node(Node &&other) {
  _is_large = other._is_large;
  data = other.data;
  other.data.map_ptr = nullptr;
  other.data.vector_ptr = nullptr;
}
Node &Node::operator=(Node &&other) {
  if (this != &other) {
    this->~Node();
    _is_large = other._is_large;
    data = other.data;
    other.data.map_ptr = nullptr;
    other.data.vector_ptr = nullptr;
  }
  return *this;
}
Node::~Node() {
  if (_is_large)
    delete data.vector_ptr;
  else
    delete data.map_ptr;
}
uint32_t Node::find(uint32_t node) {
  if (_is_large) {
    if (data.vector_ptr == nullptr)
      return NOT_FOUND;
    for (auto &[child, offset] : *data.vector_ptr) {
      uint32_t ans = child->find(node);
      if (ans != NOT_FOUND)
        return ans + offset;
    }
  } else {
    if (data.map_ptr == nullptr)
      return NOT_FOUND;
    auto it = data.map_ptr->find(node);
    if (it != data.map_ptr->end())
      return it->second;
  }
  return NOT_FOUND;
}

void Node::fill(Node &from, uint32_t dist) {
  if (!_is_large) {
    if (data.map_ptr == nullptr)
      data.map_ptr = new std::unordered_map<uint32_t, uint32_t>;
    if (from._is_large) {
      for (auto &i : *from.data.vector_ptr) {
        this->fill(*i.first, i.second + dist);
      }
    } else {
      for (const auto &[key, val] : *from.data.map_ptr) {
        data.map_ptr->try_emplace(key, val + dist);
      }
    }
  } else {
    if (data.vector_ptr == nullptr)
      data.vector_ptr = new std::vector<std::pair<Node *, uint32_t>>;
    data.vector_ptr->push_back({&from, dist});
  }
}
// При is_large < LARGE m хранит расстояния до вершин, иначе расстояния до
// компонент

void generate_unordered_temporal_graph(const std::string &filename,
                                       uint32_t num_v, uint32_t num_e,
                                       uint32_t period) {
  std::ofstream file(filename);
  // Создаем генератор случайных чисел
  std::random_device rd;  // Источник энтропии
  std::mt19937 gen(rd()); // Генератор

  // Задаем диапазон
  std::uniform_int_distribution<uint32_t> dist(0, num_v);

  for (uint32_t i = 0; i < num_e; ++i) {
    uint32_t node1 = dist(gen);
    uint32_t node2 = dist(gen);
    while (node1 == node2)
      node2 = dist(gen);
    uint32_t time = dist(gen) % period;
    file << node1 << " , " << node2 << " , " << time << '\n';
  }
}

void generate_ordered_temporal_graph(const std::string &filename,
                                     uint32_t num_v, uint32_t num_e,
                                     uint32_t period) {
  std::ofstream file(filename);
  // Создаем генератор случайных чисел
  std::random_device rd;  // Источник энтропии
  std::mt19937 gen(rd()); // Генератор

  // Задаем диапазон
  std::uniform_int_distribution<uint32_t> dist(0, num_v - 1);

  uint32_t pred = num_e / period;

  for (uint32_t j = 1; j < period; ++j) {
    for (uint32_t i = 0; i < pred; ++i) {
      uint32_t node1 = dist(gen);
      uint32_t node2 = dist(gen);
      while (node1 == node2)
        node2 = dist(gen);
      file << node1 << " , " << node2 << " , " << j << '\n';
    }
  }
}

void path_ordered_finder(std::vector<uint32_t> &path,
                         const std::string &filename, uint32_t start_node,
                         uint32_t num_v, uint32_t num_e, uint32_t period) {

  std::ifstream file(filename);

  uint32_t start_time = INF;
  uint32_t node1, node2, time;
  char _;
  path[start_node] = 0;

  while (!file.eof()) {
    file >> node1 >> _ >> node2 >> _ >> time;
    if (start_time == INF) {
      if (node1 == start_node) {
        start_time = time;
        path[node2] = 1;
      }
    } else {
      if (path[node1] != INF && path[node2] == INF &&
          path[node1] != time - start_time + 1) {
        path[node2] = time - start_time + 1;
      }
    }
  }
  file.close();
}

void path_fill(std::vector<std::unordered_map<uint32_t, uint32_t>> &path,
               std::vector<uint32_t> &node_times, const uint32_t node_from,
               const uint32_t node_to, const uint32_t cur_time) {
  if (node_times[node_to] == INF)
    node_times[node_to] = cur_time;
  uint32_t delta_time = cur_time - node_times[node_from];
  path[node_to][node_from] = delta_time;
  for (const auto &[key, val] : path[node_from]) {
    path[node_to].try_emplace(key, val + delta_time);
  }
}

void path_ordered_finder_adjacency_map(
    std::vector<std::unordered_map<uint32_t, uint32_t>> &path,
    const std::string &filename, const uint32_t num_v) {
  std::vector<uint32_t> node_times(num_v, INF);
  std::ifstream file(filename);
  uint32_t node_from, node_to, time;
  char _;
  file >> node_from >> _ >> node_to >> _ >> time;
  node_times[node_from] = time - 1;
  path_fill(path, node_times, node_from, node_to, time);
  while (file >> node_from >> _ >> node_to >> _ >> time) {
    if (!path[node_from].count(node_to)) {
      path_fill(path, node_times, node_from, node_to, time);
    }
  }
}

void path_fill_node(std::vector<Node> &path, std::vector<uint32_t> &node_times,
                    const uint32_t node_from, const uint32_t node_to,
                    const uint32_t cur_time) {
  if(node_times[node_from] == INF){
    node_times[node_from] = cur_time - 1;
    path[node_from].data.map_ptr = new std::unordered_map<uint32_t, uint32_t>;
  }
  uint32_t delta_time = cur_time - node_times[node_from];
  if (node_times[node_to] == INF) { // Новая вершина
    if (path[node_from].data.map_ptr->size() >= LARGE ||
        path[node_from]._is_large) { // большая
      path[node_to]._is_large = true;
      path[node_to].data.vector_ptr =
          new std::vector<std::pair<Node *, uint32_t>>;
      path[node_to].data.vector_ptr->push_back(
          std::pair<Node *, uint32_t>(&path[node_from], delta_time));
    } else { // маленькая
      path[node_to]._is_large = false;
      path[node_to].data.map_ptr = new std::unordered_map<uint32_t, uint32_t>;
      for (const auto &[key, val] : *path[node_from].data.map_ptr) {
        path[node_to].data.map_ptr->try_emplace(key, val + delta_time);
      }
      path[node_to].data.map_ptr->try_emplace(node_from, delta_time);
    }
    node_times[node_to] = cur_time;
  } else {                          // старая веришна
    if (!path[node_to]._is_large) { // маленькая
      path[node_to].data.map_ptr->try_emplace(node_from, delta_time);
      (*path[node_to].data.map_ptr)[node_from] = delta_time;
    }
    path[node_to].fill(path[node_from], delta_time);
  }
}

void path_ordered_finder_node(std::vector<Node> &path,
                              const std::string &filename,
                              const uint32_t num_v) {
  std::vector<uint32_t> node_times(num_v, INF);
  std::ifstream file(filename);
  uint32_t node_from, node_to, time;
  char _;
  while (file >> node_from >> _ >> node_to >> _ >> time) {
    if (path[node_to].find(node_from) == NOT_FOUND) {
      path_fill_node(path, node_times, node_from, node_to, time);
    }
  }
}


void print_memory_usage() {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::cout << line << '\t';
        }
    }
}
