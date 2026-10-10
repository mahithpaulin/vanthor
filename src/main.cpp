#include "uci.h"
#include "bitboard.h"
#include "position.h"
#include <iostream>
#include <string>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    Attacks::init();
    Position::init_zobrist();
    Uci uci;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!uci.handle_line(line)) break;
        std::cout << std::flush;
    }
    return 0;
}
