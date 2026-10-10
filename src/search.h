// VANTHOR search.h — v0.1.0 plain alpha-beta + quiescence (ID/PVS/TT in v0.2.0).
#pragma once
#include "position.h"
#include <atomic>
#include <chrono>
#include <cstdint>

struct SearchLimits {
    int depth = 64;                 // max depth if no time limit
    uint64_t max_nodes = UINT64_MAX;
    int64_t time_ms = -1;           // -1 = no time limit
    bool infinite = false;
};

struct SearchResult {
    Move best = NO_MOVE;
    Score score = 0;
    uint64_t nodes = 0;
    int depth = 0;
};

class Searcher {
public:
    SearchResult go(const Position& root, const SearchLimits& lim);
    void stop() { stop_ = true; }

private:
    Position pos_;
    std::atomic<bool> stop_{false};
    uint64_t nodes_ = 0;
    uint64_t max_nodes_ = UINT64_MAX;
    std::chrono::steady_clock::time_point deadline_;
    bool has_deadline_ = false;

    void poll();
    int search(int depth, int alpha, int beta, int ply);
    int quiescence(int alpha, int beta, int ply);
    static int move_score(const Position& pos, Move m);
};
