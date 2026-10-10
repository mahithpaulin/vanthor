// VANTHOR bitboard.h — attack generation. Original implementation.
#pragma once
#include "types.h"

namespace Attacks {
    void init();  // deterministic (fixed-seed magic search)
    Bitboard knight(int sq);
    Bitboard king(int sq);
    Bitboard pawn(Color c, int sq);          // squares a pawn on sq attacks
    Bitboard bishop(int sq, Bitboard occ);
    Bitboard rook(int sq, Bitboard occ);
    Bitboard queen(int sq, Bitboard occ);
    // squares between a and b exclusive (0 if not aligned)
    Bitboard between(int a, int b);
    // ray from sq through to the edge, exclusive of sq
    Bitboard ray(int sq, int delta);
}

int popcount(Bitboard b);
int lsb(Bitboard b);   // index of least significant bit (b != 0)
int poplsb(Bitboard& b);

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = 0x8080808080808080ULL;
