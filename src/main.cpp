#include "uci.h"
#include <iostream>
#include <string>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    Uci uci;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!uci.handle_line(line)) break;
        std::cout << std::flush;
    }
    return 0;
}
