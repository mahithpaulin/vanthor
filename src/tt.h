// VANTHOR tt.h — lock-free transposition table (Lazy SMP safe).
#pragma once
#include "types.h"
#include <atomic>
#include <cstdint>
#include <memory>

enum TTFlag : uint8_t { TT_NONE = 0, TT_EXACT = 1, TT_LOWER = 2, TT_UPPER = 3 };

// data packing (64 bit): move[0:32] | score_i16[32:48] | depth[48:56] | flag[56:64]
struct TTSlot {
    std::atomic<Key> key{0};
    std::atomic<uint64_t> data{0};
};

struct TTEntry {
    Move move = NO_MOVE;
    Score score = 0;
    uint8_t depth = 0;
    TTFlag flag = TT_NONE;
    bool hit = false;
};

class TT {
public:
    TT() { resize(16); }
    void resize(int megabytes);
    void clear();
    size_t size() const { return size_; }

    TTEntry probe(Key key);
    void store(Key key, Move move, Score score, uint8_t depth, TTFlag flag);

private:
    std::unique_ptr<TTSlot[]> table_;  // atomics: no vector (non-movable)
    size_t size_ = 0;
    size_t mask_ = 0;

    static uint64_t pack(Move m, Score s, uint8_t d, TTFlag f);
    static void unpack(uint64_t data, Move& m, Score& s, uint8_t& d, TTFlag& f);
};
