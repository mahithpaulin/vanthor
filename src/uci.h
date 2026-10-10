// VANTHOR uci.h — async protocol loop (search runs on a worker thread).
#pragma once
#include "position.h"
#include "search.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class Uci {
public:
    Uci();
    ~Uci();
    bool handle_line(const std::string& line);
    Position& position() { return pos_; }

private:
    Position pos_;
    Search search_;
    SearchOptions opts_;
    int threads_ = 1;

    std::thread worker_;
    std::atomic<bool> searching_{false};
    std::mutex out_m_;       // serializes all stdout output
    std::atomic<bool> pending_ready_{false};  // isready arrived mid-search

    void cmd_uci();
    void cmd_position(const std::string& args);
    void cmd_go(const std::string& args);
    void cmd_perft(const std::string& args);
    void cmd_divide(const std::string& args);
    void cmd_bench();
    void cmd_display();
    void cmd_eval();
    void cmd_keytest(const std::string& args);
    void cmd_nnue_verify(const std::string& args);
    bool play_uci_moves(const std::string& moves);
    void stop_search();  // signal + join (no output)
    void print_info(const InfoLine& il);
};
