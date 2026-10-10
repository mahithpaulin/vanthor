// VANTHOR movegen.cpp — move generation + SEE. Original implementation.
#include "movegen.h"
#include "bitboard.h"

namespace {
    void add_pawn_move(MoveList& ml, int from, int to, int flags) {
        int r = rank_of(to);
        if (r == 0 || r == 7) {
            ml.add(make_move_flags(from, to, flags | promo_flag(KNIGHT)));
            ml.add(make_move_flags(from, to, flags | promo_flag(BISHOP)));
            ml.add(make_move_flags(from, to, flags | promo_flag(ROOK)));
            ml.add(make_move_flags(from, to, flags | promo_flag(QUEEN)));
        } else {
            ml.add(make_move_flags(from, to, flags));
        }
    }
}

void generate_pseudo(const Position& pos, MoveList& ml, bool captures_only) {
    ml.n = 0;
    Color us = pos.side(), them = opposite(us);
    Bitboard occ = pos.occ_all();
    Bitboard own = pos.occ(us);
    Bitboard enemy = pos.occ(them);

    // ---- pawns ----
    Bitboard pawns = pos.pieces(us, PAWN);
    int up = (us == WHITE) ? 8 : -8;
    int start_rank = (us == WHITE) ? 1 : 6;
    while (pawns) {
        int from = poplsb(pawns);
        int f = file_of(from), r = rank_of(from);
        // captures (incl. en passant)
        Bitboard atks = Attacks::pawn(us, from);
        Bitboard caps = atks & enemy;
        while (caps) {
            int to = poplsb(caps);
            add_pawn_move(ml, from, to, F_CAPTURE);
        }
        if (pos.ep() != NO_SQ && (atks & (1ULL << pos.ep()))) {
            // verify an enemy pawn can actually be captured (it just double-pushed)
            add_pawn_move(ml, from, pos.ep(), F_CAPTURE | F_EP);
        }
        if (!captures_only) {
            int one = from + up;
            if (one >= 0 && one < 64 && !(occ & (1ULL << one))) {
                add_pawn_move(ml, from, one, 0);
                int two = from + 2 * up;
                if (r == start_rank && !(occ & (1ULL << two)))
                    ml.add(make_move_flags(from, two, F_DOUBLE));
            }
        }
        (void)f;
    }

    auto add_moves = [&](int from, Bitboard targets, int extra_flags) {
        while (targets) {
            int to = poplsb(targets);
            int flags = extra_flags;
            if (enemy & (1ULL << to)) flags |= F_CAPTURE;
            // in captures_only mode skip quiet moves
            if (captures_only && !(flags & F_CAPTURE)) continue;
            ml.add(make_move_flags(from, to, flags));
        }
    };

    // ---- knights ----
    Bitboard knights = pos.pieces(us, KNIGHT);
    while (knights) {
        int from = poplsb(knights);
        add_moves(from, Attacks::knight(from) & ~own, 0);
    }
    // ---- king (no castling here) ----
    {
        int ks = pos.king_sq(us);
        if (ks != NO_SQ) add_moves(ks, Attacks::king(ks) & ~own, 0);
    }
    // ---- sliders ----
    Bitboard bishops = pos.pieces(us, BISHOP);
    while (bishops) {
        int from = poplsb(bishops);
        add_moves(from, Attacks::bishop(from, occ) & ~own, 0);
    }
    Bitboard rooks = pos.pieces(us, ROOK);
    while (rooks) {
        int from = poplsb(rooks);
        add_moves(from, Attacks::rook(from, occ) & ~own, 0);
    }
    Bitboard queens = pos.pieces(us, QUEEN);
    while (queens) {
        int from = poplsb(queens);
        add_moves(from, Attacks::queen(from, occ) & ~own, 0);
    }

    // ---- castling (quiets only) ----
    if (!captures_only) {
        if (us == WHITE) {
            if ((pos.castling() & WK_CASTLE) &&
                !(occ & ((1ULL << F1) | (1ULL << G1))) &&
                !pos.is_attacked(E1, BLACK) && !pos.is_attacked(F1, BLACK) &&
                !pos.is_attacked(G1, BLACK))
                ml.add(make_move_flags(E1, G1, F_CASTLE));
            if ((pos.castling() & WQ_CASTLE) &&
                !(occ & ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))) &&
                !pos.is_attacked(E1, BLACK) && !pos.is_attacked(D1, BLACK) &&
                !pos.is_attacked(C1, BLACK))
                ml.add(make_move_flags(E1, C1, F_CASTLE));
        } else {
            if ((pos.castling() & BK_CASTLE) &&
                !(occ & ((1ULL << F8) | (1ULL << G8))) &&
                !pos.is_attacked(E8, WHITE) && !pos.is_attacked(F8, WHITE) &&
                !pos.is_attacked(G8, WHITE))
                ml.add(make_move_flags(E8, G8, F_CASTLE));
            if ((pos.castling() & BQ_CASTLE) &&
                !(occ & ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))) &&
                !pos.is_attacked(E8, WHITE) && !pos.is_attacked(D8, WHITE) &&
                !pos.is_attacked(C8, WHITE))
                ml.add(make_move_flags(E8, C8, F_CASTLE));
        }
    }
}

void generate_legal(const Position& pos, MoveList& ml) {
    MoveList pseudo;
    generate_pseudo(pos, pseudo, false);
    ml.n = 0;
    Color us = pos.side();
    Position& mut = const_cast<Position&>(pos);
    for (int i = 0; i < pseudo.n; i++) {
        mut.do_move(pseudo.m[i]);
        if (!mut.in_check(us)) ml.add(pseudo.m[i]);
        mut.undo_move(pseudo.m[i]);
    }
}

void generate_legal_captures(const Position& pos, MoveList& ml) {
    MoveList pseudo;
    generate_pseudo(pos, pseudo, true);
    ml.n = 0;
    Color us = pos.side();
    Position& mut = const_cast<Position&>(pos);
    for (int i = 0; i < pseudo.n; i++) {
        mut.do_move(pseudo.m[i]);
        if (!mut.in_check(us)) ml.add(pseudo.m[i]);
        mut.undo_move(pseudo.m[i]);
    }
}

// ---- SEE (standard swap algorithm, piece values) ----
namespace {
    int SEE_VAL[6] = {100, 320, 330, 500, 900, 20000};

    // least valuable attacker of `by` on sq; returns square or NO_SQ
    int smallest_attacker(const Position& pos, Bitboard occ, int sq, Color by) {
        Bitboard b;
        b = Attacks::pawn(opposite(by), sq) & pos.pieces(by, PAWN) & occ;
        if (b) return lsb(b);
        b = Attacks::knight(sq) & pos.pieces(by, KNIGHT) & occ;
        if (b) return lsb(b);
        b = Attacks::bishop(sq, occ) & pos.pieces(by, BISHOP) & occ;
        if (b) return lsb(b);
        b = Attacks::rook(sq, occ) & pos.pieces(by, ROOK) & occ;
        if (b) return lsb(b);
        b = Attacks::queen(sq, occ) & pos.pieces(by, QUEEN) & occ;
        if (b) return lsb(b);
        b = Attacks::king(sq) & pos.pieces(by, KING) & occ;
        if (b) return lsb(b);
        return NO_SQ;
    }
}

int see_capture(const Position& pos, Move m) {
    // Net material gain for the mover of a capture (standard swap algorithm).
    // Pins are ignored (standard SEE limitation); king may capture last.
    if (!move_is_capture(m)) return 0;
    int from = move_from(m), to = move_to(m);
    int apc = pos.piece_on(from);
    if (apc < 0) return 0;
    apc %= 6;
    int vpc;
    Bitboard occ = pos.occ_all() & ~(1ULL << from);
    if (move_is_ep(m)) {
        vpc = PAWN;
        int cap = to + (pos.side() == WHITE ? -8 : 8);
        occ &= ~(1ULL << cap);
    } else {
        int v = pos.piece_on(to);
        if (v < 0) return 0;
        vpc = v % 6;
        occ &= ~(1ULL << to);
    }
    Bitboard occ_to = occ | (1ULL << to);  // attacker now stands on `to`
    int gain[32];
    int d = 0;
    gain[0] = SEE_VAL[vpc];
    int last_val = SEE_VAL[apc];
    Color stm = opposite(pos.side());
    while (true) {
        int atk = smallest_attacker(pos, occ_to, to, stm);
        if (atk == NO_SQ) break;
        int p = pos.piece_on(atk) % 6;
        occ_to &= ~(1ULL << atk);
        ++d;
        gain[d] = last_val - gain[d - 1];
        last_val = SEE_VAL[p];
        stm = opposite(stm);
        if (d >= 31) break;
    }
    while (d) {
        --d;
        int a = -gain[d];
        gain[d] = (a > gain[d + 1]) ? -a : -gain[d + 1];
    }
    return gain[0];
}
