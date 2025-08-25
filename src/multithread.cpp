#include <iostream>
#include <cstdlib>
#include <vector>
#include <random>
#include <fstream>
#include <chrono>
#include <sstream>
#include <thread>
#include <string>

#define INF 1e9

void run(std::string s) {
    std::cout << s << '\n';
}   

int main(){
    std::thread t1(run, "a");
    std::thread t2(run, "b");
    t2.join();
    t1.join();
    return 0;

}
