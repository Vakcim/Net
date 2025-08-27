#include <iostream>
#include <cstdlib>
#include <random>
#include <fstream>
#include <chrono>
#include <sstream>
#include <thread>
#include <queue>
#include "net.h"


    Node::~Node(){
        if(_is_large) delete data.vector_ptr;
        else delete data.map_ptr;
    }
    unsigned Node::find(unsigned node){
        if(_is_large){
            unsigned ans;
            for(auto i : *data.vector_ptr){
                ans = i.first.find(node);
                if(ans != NOT_FOUND) return ans + i.second;
            }
        } else if(data.map_ptr->count(node))  return (*data.map_ptr)[node];
        return NOT_FOUND;
    }

    void Node::fill(Node &from, unsigned dist){
        if(_is_large){
            if(from._is_large){
                for(auto i : *from.data.vector_ptr){
                    this->fill(i.first, i.second+dist);
                }
            } else {
                for (const auto& [key, val] : *from.data.map_ptr) { 
                    data.map_ptr->try_emplace(key, val + dist);
                }
            }
        } else {
            data.vector_ptr->push_back(std::pair<Node, unsigned>(from,  dist));
        }
        
    }
//При is_large < LARGE m хранит расстояния до вершин, иначе расстояния до компонент

void generate_unordered_temporal_graph(const std::string &filename, unsigned num_v,
        unsigned num_e, unsigned period){
    std::ofstream file(filename);
    //Создаем генератор случайных чисел
    std::random_device rd;  // Источник энтропии 
    std::mt19937 gen(rd()); // Генератор

    //Задаем диапазон
    std::uniform_int_distribution<unsigned> dist(0, num_v);

    for(unsigned i= 0; i < num_e; ++i){
        unsigned node1 = dist(gen);
        unsigned node2 = dist(gen);
        while(node1 == node2) node2 = dist(gen);
        unsigned time = dist(gen)%period;
        file << node1 << " , " << node2 << " , " << time << '\n';
    } 
}

void generate_ordered_temporal_graph(const std::string &filename, unsigned num_v,
        unsigned num_e, unsigned period){
    std::ofstream file(filename);
    //Создаем генератор случайных чисел
    std::random_device rd;  // Источник энтропии 
    std::mt19937 gen(rd()); // Генератор

    //Задаем диапазон
    std::uniform_int_distribution<unsigned> dist(0, num_v-1);

    unsigned pred = num_e/period;
    
    for(unsigned j = 0; j < period; ++j){
        for(unsigned i= 0; i < pred; ++i){
            unsigned node1 = dist(gen);
            unsigned node2 = dist(gen);
            while(node1 == node2) node2 = dist(gen);
            file << node1 << " , " << node2 << " , " << j << '\n';
        } 
    }
}


template <typename F, typename... Args>
void calc_time(F&& func, Args&&... args){
    auto start = std::chrono::high_resolution_clock::now();
    func(std::forward<Args>(args)...);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "function woring for: " << duration.count()/1000000 << '.' << duration.count()%1000000 << " seconds\n";
}

void path_ordered_finder(std::vector<unsigned> &path, const std::string &filename, unsigned start_node, unsigned num_v,
        unsigned num_e, unsigned period) {

    std::ifstream file(filename);

    unsigned start_time = INF;
    unsigned node1, node2, time; char _;
    path[start_node] = 0;

    while (!file.eof()) {
        file >> node1 >> _ >> node2 >> _ >> time;
        if(start_time == INF){
            if(node1 == start_node){
                start_time = time;
                path[node2] = 1;
            }
        }  else {
            if(path[node1] != INF && path[node2] == INF && path[node1] != time - start_time + 1){
                path[node2] = time - start_time + 1;
            }
        }
    }
    file.close();
}

void path_fill(std::vector<std::unordered_map<unsigned, unsigned>> &path, std::vector<unsigned> &node_times,
                const unsigned node_from, const unsigned node_to, const unsigned cur_time)  {
    if(node_times[node_to] == INF) node_times[node_to] = cur_time;
    unsigned delta_time = cur_time - node_times[node_from];
    path[node_to][node_from] = delta_time;
    for (const auto& [key, val] : path[node_from]) { 
        path[node_to].try_emplace(key, val + delta_time);
    }
}

void path_ordered_finder_adjacency_map(std::vector<std::unordered_map<unsigned, unsigned>> &path, const std::string &filename, const unsigned num_v){
    std::vector<unsigned> node_times(num_v, INF);
    std::ifstream file(filename);
    unsigned node_from, node_to, time; char _;
    file >> node_from >> _ >> node_to >> _ >> time;
    node_times[node_from] = time - 1;
    path_fill(path, node_times, node_from, node_to, time);
    while(file >> node_from >> _ >> node_to >> _ >> time){
        if(!path[node_from].count(node_to)){
            path_fill(path, node_times, node_from, node_to, time);
        }
    }
}

void path_fill_node(std::vector<Node> &path, std::vector<unsigned> &node_times,
                const unsigned node_from, const unsigned node_to, const unsigned cur_time)  {
    unsigned delta_time = cur_time - node_times[node_from];
    if(node_times[node_to] == INF){ //Новая вершина
        if(!path[node_from]._is_large && path[node_from].data.map_ptr->size() >= LARGE || path[node_from]._is_large){ //большая
            path[node_to]._is_large = true;
            path[node_to].data.vector_ptr = new std::vector<std::pair<Node, unsigned>>;
            path[node_to].data.vector_ptr->push_back(std::pair<Node, unsigned>(path[node_from],  delta_time));
        } else { // маленькая
            path[node_to]._is_large = false;
            path[node_to].data.map_ptr = new std::map<unsigned, unsigned>;
            for (const auto& [key, val] : *path[node_from].data.map_ptr) { 
                path[node_to].data.map_ptr->try_emplace(key, val + delta_time);
            }
        }
        node_times[node_to] = cur_time;
    } else { // старая веришна
        if(!path[node_to]._is_large){ //маленькая
            (*path[node_to].data.map_ptr)[node_from] = delta_time;
        }
        path[node_to].fill(path[node_from], delta_time);
    }
}

void path_ordered_finder_node(std::vector<Node> &path, const std::string &filename, const unsigned num_v){
    std::vector<unsigned> node_times(num_v, INF);
    std::ifstream file(filename);
    unsigned node_from, node_to, time; char _;
    file >> node_from >> _ >> node_to >> _ >> time;
    node_times[node_from] = time - 1;
    path_fill_node(path, node_times, node_from, node_to, time);
    while(file >> node_from >> _ >> node_to >> _ >> time){
        if(!path[node_from].find(node_to) != NOT_FOUND){
            path_fill_node(path, node_times, node_from, node_to, time);
        }
    }
}