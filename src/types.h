// VANTHOR types.h — basic types. Original implementation for Project Vanthor.
#pragma once
#include <cstdint>
#include <string>

using Bitboard = uint64_t;
using Key = uint64_t;

enum Color : int { WHITE = 0, BLACK = 1, N_COLORS = 2 };
enum Piece : int { PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5, NO_PIECE = 6 };

// Squares: A1=0 .. H8=63 (little-endian rank-file mapping)
enum Square : int {
    A1=0,B1,C1,D1,E1,F1,G1,H1,
    A2,B2,C2,D2,E2,F2,G2,H2,
    A3,B3,C3,D3,E3,F3,G3,H3,
    A4,B4,C4,D4,E4,F4,G4,H4,
    A5,B5,C5,D5,E5,F5,G5,H5,
    A6,B6,C6,D6,E6,F6,G6,H6,
    A7,B7,C7,D7,E7,F7,G7,H7,
    A8,B8,C8,D8,E8,F8,G8,H8,
    NO_SQ = 64
};

inline int file_of(int sq) { return sq & 7; }
inline int rank_of(int sq) { return sq >> 3; }
inline int make_sq(int file, int rank) { return rank * 8 + file; }
inline Color opposite(Color c) { return Color(c ^ 1); }

// ---- Move encoding (uint32_t) ----
// bits 0-5: from, 6-11: to, 12-14: promo piece (0=none, else PAWN..KING enum)
// bit 15: capture, bit 16: double push, bit 17: en passant capture, bit 18: castle
using Move = uint32_t;
constexpr Move NO_MOVE = 0;

inline Move make_move(int from, int to) { return Move(from | (to << 6)); }
inline Move make_move_flags(int from, int to, int flags) { return Move(from | (to << 6) | flags); }
inline int move_from(Move m) { return m & 63; }
inline int move_to(Move m) { return (m >> 6) & 63; }
inline int move_promo(Move m) { return (m >> 12) & 7; }
inline bool move_is_capture(Move m) { return (m >> 15) & 1; }
inline bool move_is_double(Move m) { return (m >> 16) & 1; }
inline bool move_is_ep(Move m) { return (m >> 17) & 1; }
inline bool move_is_castle(Move m) { return (m >> 18) & 1; }
inline bool move_is_promo(Move m) { return move_promo(m) != 0; }

constexpr int F_CAPTURE = 1 << 15;
constexpr int F_DOUBLE  = 1 << 16;
constexpr int F_EP      = 1 << 17;
constexpr int F_CASTLE  = 1 << 18;
inline int promo_flag(Piece p) { return (int(p) << 12); }

// ---- Scores ----
using Score = int;
constexpr Score INF = 33000;
constexpr Score MATE = 32000;
constexpr Score DRAW_SCORE = 0;

// Castling rights bits
constexpr int WK_CASTLE = 1, WQ_CASTLE = 2, BK_CASTLE = 4, BQ_CASTLE = 8;

std::string sq_name(int sq);
std::string move_name(Move m);
