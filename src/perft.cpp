// VANTHOR perft.cpp
#include "perft.h"
#include "movegen.h"

uint64_t perft(Position& pos, int depth) {
    if (depth == 0) return 1;
    MoveList ml;
    generate_legal(pos, ml);
    if (depth == 1) return (uint64_t)ml.n;
    uint64_t nodes = 0;
    for (int i = 0; i < ml.n; i++) {
        pos.do_move(ml.m[i]);
        nodes += perft(pos, depth - 1);
        pos.undo_move(ml.m[i]);
    }
    return nodes;
}
