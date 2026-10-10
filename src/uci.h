// VANTHOR uci.h — protocol loop over a real position + searcher.
#pragma once
#include "position.h"
#include "search.h"
#include <string>

class Uci {
public:
    Uci();
    // handle one input line; returns false when the loop should exit
    bool handle_line(const std::string& line);
    Position& position() { return pos_; }

private:
    Position pos_;
    Searcher searcher_;
    int threads_ = 1;
    int hash_mb_ = 16;

    void cmd_uci();
    void cmd_position(const std::string& args);
    void cmd_go(const std::string& args);
    void cmd_perft(const std::string& args);
    void cmd_divide(const std::string& args);
    void cmd_bench();
    void cmd_display();
    void cmd_eval();
    bool play_uci_moves(const std::string& moves);
};
