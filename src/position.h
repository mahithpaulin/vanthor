// VANTHOR position.h — board state + make/unmake. Original implementation.
#pragma once
#include "types.h"
#include <vector>

struct Undo {
    Move move = NO_MOVE;
    int captured = NO_PIECE;   // piece type captured (NO_PIECE if none)
    int castling = 0;
    int ep = NO_SQ;
    int half = 0;
    Key key = 0;
};

class Position {
public:
    Position();
    bool set_fen(const std::string& fen);  // false on parse error
    std::string fen() const;

    // accessors
    Color side() const { return side_; }
    int castling() const { return castling_; }
    int ep() const { return ep_; }
    int half() const { return half_; }
    int full() const { return full_; }
    Key key() const { return key_; }
    Bitboard pieces(Color c, Piece p) const { return bb_[c][p]; }
    Bitboard occ(Color c) const { return occ_[c]; }
    Bitboard occ_all() const { return occ_[WHITE] | occ_[BLACK]; }
    int piece_on(int sq) const { return board_[sq]; }  // -1 empty else c*6+p
    int king_sq(Color c) const;

    void do_move(Move m);
    void undo_move(Move m);
    const Undo& last_undo() const { return history_.back(); }

    bool in_check(Color c) const;
    bool is_attacked(int sq, Color by) const;
    int repetitions() const;  // occurrences of current key in game history

    static void init_zobrist();

private:
    Bitboard bb_[2][6] = {};
    Bitboard occ_[2] = {};
    int board_[64];
    Color side_ = WHITE;
    int castling_ = 0;
    int ep_ = NO_SQ;
    int half_ = 0, full_ = 1;
    Key key_ = 0;
    std::vector<Undo> history_;
    std::vector<Key> game_keys_;  // keys after each position (incl. initial)

    void put(int sq, Color c, Piece p);
    void remove(int sq);
    void move_piece(int from, int to);
    Key compute_key() const;
};
