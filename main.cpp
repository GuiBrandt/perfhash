#include "perfhash.hpp"

#include <list>
#include <string>

#include <iostream>

using namespace hashing;

int main(int argc, char** argv) {
    std::cout << "[start]" << std::endl;

    std::vector<std::pair<int, std::string>> l;
    l.push_back(std::make_pair(1, "a"));
    l.push_back(std::make_pair(3, "b"));
    l.push_back(std::make_pair(9, "c"));

    perfect_hash_map<int, std::string> h(l.begin(), l.end());
    std::cout << h.at(1) << std::endl;
    std::cout << h.at(3) << std::endl;
    std::cout << h.at(9) << std::endl;

    h[3] = "teste";

    std::cout << h.at(3) << std::endl;

    try {
        std::cout << h.at(5) << std::endl;
    } catch (std::out_of_range e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "[end]" << std::endl;
    std::cout << "[start]" << std::endl;

    return 0;
}