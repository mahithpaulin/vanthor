// VANTHOR movegen.h — move generation. Original implementation.
#pragma once
#include "position.h"

struct MoveList {
    Move m[256];
    int n = 0;
    void add(Move mv) { m[n++] = mv; }
};

// pseudo-legal moves; if captures_only, quiets (non-captures) are skipped
// (promotions count as captures=false unless capturing... note: quiet
// promotions ARE generated in captures_only mode for quiescence correctness)
void generate_pseudo(const Position& pos, MoveList& ml, bool captures_only);
void generate_legal(const Position& pos, MoveList& ml);
void generate_legal_captures(const Position& pos, MoveList& ml);

// static exchange evaluation of a capture move: net material gain for mover
int see_capture(const Position& pos, Move m);
