// VANTHOR position.cpp — board state + make/unmake. Original implementation.
#include "position.h"
#include "bitboard.h"
#include <sstream>

namespace {
    Key ZOB_PIECE[2][6][64];
    Key ZOB_CASTLE[16];
    Key ZOB_EP[8];
    Key ZOB_SIDE;
    bool ZOB_INIT = false;

    uint64_t splitmix64(uint64_t& s) {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // castle_mask[sq]: rights surviving when a piece moves from/to sq
    int CASTLE_MASK[64];
    void init_castle_mask() {
        for (int i = 0; i < 64; i++) CASTLE_MASK[i] = 15;
        CASTLE_MASK[E1] = 12; CASTLE_MASK[A1] = 13; CASTLE_MASK[H1] = 14;
        CASTLE_MASK[E8] = 3;  CASTLE_MASK[A8] = 7;  CASTLE_MASK[H8] = 11;
    }
}

void Position::init_zobrist() {
    if (ZOB_INIT) return;
    uint64_t s = 0xD1B54A32D192ED03ULL;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            for (int sq = 0; sq < 64; sq++)
                ZOB_PIECE[c][p][sq] = splitmix64(s);
    for (int i = 0; i < 16; i++) ZOB_CASTLE[i] = splitmix64(s);
    for (int i = 0; i < 8; i++) ZOB_EP[i] = splitmix64(s);
    ZOB_SIDE = splitmix64(s);
    init_castle_mask();
    ZOB_INIT = true;
}

Position::Position() {
    for (int i = 0; i < 64; i++) board_[i] = -1;
    set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Position::put(int sq, Color c, Piece p) {
    bb_[c][p] |= 1ULL << sq;
    occ_[c] |= 1ULL << sq;
    board_[sq] = int(c) * 6 + int(p);
    key_ ^= ZOB_PIECE[c][p][sq];
}

void Position::remove(int sq) {
    int v = board_[sq];
    Color c = Color(v / 6);
    Piece p = Piece(v % 6);
    bb_[c][p] &= ~(1ULL << sq);
    occ_[c] &= ~(1ULL << sq);
    board_[sq] = -1;
    key_ ^= ZOB_PIECE[c][p][sq];
}

void Position::move_piece(int from, int to) {
    int v = board_[from];
    Color c = Color(v / 6);
    Piece p = Piece(v % 6);
    bb_[c][p] &= ~(1ULL << from);
    occ_[c] &= ~(1ULL << from);
    key_ ^= ZOB_PIECE[c][p][from];
    bb_[c][p] |= 1ULL << to;
    occ_[c] |= 1ULL << to;
    board_[from] = -1;
    board_[to] = v;
    key_ ^= ZOB_PIECE[c][p][to];
}

Key Position::compute_key() const {
    Key k = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) {
            Bitboard b = bb_[c][p];
            while (b) {
                int sq = poplsb(b);
                k ^= ZOB_PIECE[c][p][sq];
            }
        }
    k ^= ZOB_CASTLE[castling_];
    if (ep_ != NO_SQ) k ^= ZOB_EP[file_of(ep_)];
    if (side_ == BLACK) k ^= ZOB_SIDE;
    return k;
}

bool Position::set_fen(const std::string& fen) {
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) bb_[c][p] = 0;
    occ_[0] = occ_[1] = 0;
    for (int i = 0; i < 64; i++) board_[i] = -1;
    key_ = 0;
    history_.clear();
    game_keys_.clear();

    std::istringstream in(fen);
    std::string board, stm, cast, epsq, half, full;
    if (!(in >> board >> stm >> cast >> epsq >> half >> full)) return false;

    int rank = 7, file = 0;
    for (char ch : board) {
        if (ch == '/') { rank--; file = 0; continue; }
        if (ch >= '1' && ch <= '8') { file += ch - '0'; continue; }
        Color c = (ch >= 'A' && ch <= 'Z') ? WHITE : BLACK;
        Piece p;
        switch (ch | 32) {
            case 'p': p = PAWN; break; case 'n': p = KNIGHT; break;
            case 'b': p = BISHOP; break; case 'r': p = ROOK; break;
            case 'q': p = QUEEN; break; case 'k': p = KING; break;
            default: return false;
        }
        if (file > 7 || rank < 0) return false;
        put(make_sq(file, rank), c, p);
        file++;
    }
    side_ = (stm == "b") ? BLACK : WHITE;
    castling_ = 0;
    if (cast != "-") {
        for (char ch : cast) {
            if (ch == 'K') castling_ |= WK_CASTLE;
            else if (ch == 'Q') castling_ |= WQ_CASTLE;
            else if (ch == 'k') castling_ |= BK_CASTLE;
            else if (ch == 'q') castling_ |= BQ_CASTLE;
            else return false;
        }
    }
    ep_ = NO_SQ;
    if (epsq != "-") {
        if (epsq.size() != 2) return false;
        ep_ = make_sq(epsq[0] - 'a', epsq[1] - '1');
    }
    try {
        half_ = std::stoi(half);
        full_ = std::stoi(full);
    } catch (...) { return false; }
    key_ = compute_key();
    game_keys_.push_back(key_);
    return true;
}

std::string Position::fen() const {
    std::string s;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            int v = board_[make_sq(f, r)];
            if (v < 0) { empty++; continue; }
            if (empty) { s += char('0' + empty); empty = 0; }
            static const char names[6] = {'p','n','b','r','q','k'};
            char ch = names[v % 6];
            s += (v / 6 == WHITE) ? char(ch - 32) : ch;
        }
        if (empty) s += char('0' + empty);
        if (r) s += '/';
    }
    s += (side_ == WHITE) ? " w " : " b ";
    if (!castling_) s += "-";
    else {
        if (castling_ & WK_CASTLE) s += 'K';
        if (castling_ & WQ_CASTLE) s += 'Q';
        if (castling_ & BK_CASTLE) s += 'k';
        if (castling_ & BQ_CASTLE) s += 'q';
    }
    s += " ";
    s += (ep_ == NO_SQ) ? "-" : sq_name(ep_);
    s += " " + std::to_string(half_) + " " + std::to_string(full_);
    return s;
}

int Position::king_sq(Color c) const {
    Bitboard b = bb_[c][KING];
    return b ? lsb(b) : NO_SQ;
}

bool Position::is_attacked(int sq, Color by) const {
    // pawns: use reverse lookup — a pawn of color `by` attacking sq sits on
    // a square from which `by` pawns attack sq... pawns of `by` on squares
    // that attack sq: those are exactly Attacks::pawn(opposite(by), sq)? No:
    // white pawn on X attacks sq means X is one down-diagonal from sq.
    // Attacks::pawn(BLACK, sq) gives squares a black pawn on sq would attack
    // (down-diagonals) = squares where a WHITE pawn sits that attacks sq. Yes.
    if (Attacks::pawn(opposite(by), sq) & bb_[by][PAWN]) return true;
    if (Attacks::knight(sq) & bb_[by][KNIGHT]) return true;
    if (Attacks::king(sq) & bb_[by][KING]) return true;
    Bitboard occ = occ_all();
    Bitboard diag = Attacks::bishop(sq, occ) & (bb_[by][BISHOP] | bb_[by][QUEEN]);
    if (diag) return true;
    Bitboard straight = Attacks::rook(sq, occ) & (bb_[by][ROOK] | bb_[by][QUEEN]);
    return straight != 0;
}

bool Position::in_check(Color c) const {
    int ks = king_sq(c);
    return ks != NO_SQ && is_attacked(ks, opposite(c));
}

void Position::do_move(Move m) {
    Undo u;
    u.move = m;
    u.castling = castling_;
    u.ep = ep_;
    u.half = half_;
    u.key = key_;

    int from = move_from(m), to = move_to(m);
    int v = board_[from];
    Color us = Color(v / 6);
    Piece pc = Piece(v % 6);

    if (ep_ != NO_SQ) key_ ^= ZOB_EP[file_of(ep_)];
    key_ ^= ZOB_CASTLE[castling_];

    u.captured = NO_PIECE;
    if (move_is_capture(m)) {
        if (move_is_ep(m)) {
            int cap = to + (us == WHITE ? -8 : 8);
            // guard: only remove an actual enemy pawn (malformed FEN safety)
            if (board_[cap] == int(opposite(us)) * 6 + PAWN) {
                u.captured = PAWN;
                remove(cap);
            }
        } else if (board_[to] >= 0) {
            u.captured = board_[to] % 6;
            remove(to);
        }
    }

    half_++;
    if (pc == PAWN) half_ = 0;
    if (move_is_capture(m)) half_ = 0;

    ep_ = NO_SQ;
    if (move_is_double(m)) ep_ = (from + to) / 2;

    if (move_is_castle(m)) {
        move_piece(from, to);
        // move the rook
        if (to == G1) move_piece(H1, F1);
        else if (to == C1) move_piece(A1, D1);
        else if (to == G8) move_piece(H8, F8);
        else if (to == C8) move_piece(A8, D8);
    } else if (move_is_promo(m)) {
        remove(from);
        put(to, us, Piece(move_promo(m)));
    } else {
        move_piece(from, to);
    }

    castling_ &= CASTLE_MASK[from] & CASTLE_MASK[to];
    if (us == BLACK) full_++;

    side_ = opposite(us);
    key_ ^= ZOB_SIDE;
    key_ ^= ZOB_CASTLE[castling_];
    if (ep_ != NO_SQ) key_ ^= ZOB_EP[file_of(ep_)];

    history_.push_back(u);
    game_keys_.push_back(key_);
}

void Position::undo_move(Move m) {
    (void)m;
    Undo u = history_.back();
    history_.pop_back();
    game_keys_.pop_back();

    int from = move_from(u.move), to = move_to(u.move);
    side_ = opposite(side_);
    Color us = side_;
    if (us == BLACK) full_--;

    if (move_is_castle(u.move)) {
        move_piece(to, from);
        if (to == G1) move_piece(F1, H1);
        else if (to == C1) move_piece(D1, A1);
        else if (to == G8) move_piece(F8, H8);
        else if (to == C8) move_piece(D8, A8);
    } else if (move_is_promo(u.move)) {
        remove(to);
        put(from, us, PAWN);
    } else {
        move_piece(to, from);
    }

    if (move_is_capture(u.move) && u.captured != NO_PIECE) {
        if (move_is_ep(u.move)) {
            int cap = to + (us == WHITE ? -8 : 8);
            put(cap, opposite(us), PAWN);
        } else {
            put(to, opposite(us), Piece(u.captured));
        }
    }

    castling_ = u.castling;
    ep_ = u.ep;
    half_ = u.half;
    key_ = u.key;
}

int Position::repetitions() const {
    int n = 0;
    for (Key k : game_keys_)
        if (k == key_) n++;
    return n - 1;  // occurrences before the current one
}

void Position::do_null() {
    Undo u;
    u.move = NO_MOVE;
    u.captured = NO_PIECE;
    u.castling = castling_;
    u.ep = ep_;
    u.half = half_;
    u.key = key_;
    if (ep_ != NO_SQ) key_ ^= ZOB_EP[file_of(ep_)];
    ep_ = NO_SQ;
    half_++;
    side_ = opposite(side_);
    key_ ^= ZOB_SIDE;
    history_.push_back(u);
    game_keys_.push_back(key_);
}

void Position::undo_null() {
    Undo u = history_.back();
    history_.pop_back();
    game_keys_.pop_back();
    side_ = opposite(side_);
    ep_ = u.ep;
    half_ = u.half;
    castling_ = u.castling;  // unchanged by null, restored for safety
    key_ = u.key;
}

bool Position::has_non_pawn(Color c) const {
    return (bb_[c][KNIGHT] | bb_[c][BISHOP] | bb_[c][ROOK] | bb_[c][QUEEN]) != 0;
}
