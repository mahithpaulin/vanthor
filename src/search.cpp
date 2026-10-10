// VANTHOR search.cpp — v0.1.0 baseline. Original implementation.
#include "search.h"
#include "movegen.h"
#include "evaluate.h"

namespace {
    const int VAL[7] = {100, 320, 330, 500, 900, 20000, 0};

    // simple insertion of best-first ordering by repeated max-pick
    int pick_move(MoveList& ml, int* scores, int start) {
        int best = start;
        for (int i = start + 1; i < ml.n; i++)
            if (scores[i] > scores[best]) best = i;
        Move m = ml.m[start]; ml.m[start] = ml.m[best]; ml.m[best] = m;
        int s = scores[start]; scores[start] = scores[best]; scores[best] = s;
        return start;
    }
}

int Searcher::move_score(const Position& pos, Move m) {
    if (move_is_promo(m)) return 100000 + int(move_promo(m)) * 1000;
    if (move_is_capture(m)) {
        int to = move_to(m), from = move_from(m);
        int victim = move_is_ep(m) ? PAWN : (pos.piece_on(to) % 6);
        int attacker = pos.piece_on(from) % 6;
        return 10000 + VAL[victim] * 10 - VAL[attacker] / 10;
    }
    return 0;
}

void Searcher::poll() {
    if (stop_) return;
    if (nodes_ >= max_nodes_) { stop_ = true; return; }
    if (has_deadline_ && std::chrono::steady_clock::now() >= deadline_) stop_ = true;
}

int Searcher::quiescence(int alpha, int beta, int ply) {
    if ((nodes_ & 1023) == 0) {
        poll();
        if (stop_) return alpha;
    }
    nodes_++;
    if (pos_.half() >= 100 || pos_.repetitions() >= 2) return DRAW_SCORE;
    if (ply >= 64) return evaluate(pos_);

    bool check = pos_.in_check(pos_.side());
    int stand = check ? -INF : evaluate(pos_);
    if (!check) {
        if (stand >= beta) return beta;
        if (stand > alpha) alpha = stand;
    }
    MoveList ml;
    if (check) generate_legal(pos_, ml);
    else generate_legal_captures(pos_, ml);
    int scores[256];
    for (int i = 0; i < ml.n; i++) scores[i] = move_score(pos_, ml.m[i]);
    for (int i = 0; i < ml.n; i++) {
        pick_move(ml, scores, i);
        // skip likely-losing captures when not in check (SEE pruning lite)
        if (!check && see_capture(pos_, ml.m[i]) < 0) continue;
        pos_.do_move(ml.m[i]);
        int s = -quiescence(-beta, -alpha, ply + 1);
        pos_.undo_move(ml.m[i]);
        if (stop_) return alpha;
        if (s >= beta) return beta;
        if (s > alpha) alpha = s;
    }
    if (check && ml.n == 0) return -MATE + ply;
    return alpha;
}

int Searcher::search(int depth, int alpha, int beta, int ply) {
    if ((nodes_ & 1023) == 0) {
        poll();
        if (stop_) return alpha;
    }
    nodes_++;
    if (pos_.half() >= 100 || pos_.repetitions() >= 2) return DRAW_SCORE;
    if (depth <= 0) return quiescence(alpha, beta, ply);
    if (ply >= 64) return evaluate(pos_);

    MoveList ml;
    generate_legal(pos_, ml);
    if (ml.n == 0) return pos_.in_check(pos_.side()) ? -MATE + ply : DRAW_SCORE;

    int scores[256];
    for (int i = 0; i < ml.n; i++) scores[i] = move_score(pos_, ml.m[i]);
    int best = -INF;
    for (int i = 0; i < ml.n; i++) {
        pick_move(ml, scores, i);
        pos_.do_move(ml.m[i]);
        int s = -search(depth - 1, -beta, -alpha, ply + 1);
        pos_.undo_move(ml.m[i]);
        if (stop_) return best == -INF ? alpha : best;
        if (s > best) {
            best = s;
            if (s > alpha) {
                alpha = s;
                if (alpha >= beta) break;
            }
        }
    }
    return best;
}

SearchResult Searcher::go(const Position& root, const SearchLimits& lim) {
    pos_ = root;
    stop_ = false;
    nodes_ = 0;
    max_nodes_ = lim.max_nodes;
    has_deadline_ = (lim.time_ms >= 0 && !lim.infinite);
    if (has_deadline_)
        deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(lim.time_ms);

    SearchResult r;
    MoveList ml;
    generate_legal(pos_, ml);
    if (ml.n == 0) { r.best = NO_MOVE; return r; }
    if (ml.n == 1) {
        r.best = ml.m[0];
        r.nodes = 1;
        return r;
    }
    int depth = lim.depth;
    if (depth > 64) depth = 64;
    int scores[256];
    for (int i = 0; i < ml.n; i++) scores[i] = move_score(pos_, ml.m[i]);
    int alpha = -INF;
    int best_score = -INF;
    Move best = ml.m[0];
    bool completed_one = false;
    for (int i = 0; i < ml.n; i++) {
        pick_move(ml, scores, i);
        pos_.do_move(ml.m[i]);
        int s = -search(depth - 1, -INF, -alpha, 1);
        pos_.undo_move(ml.m[i]);
        if (stop_ && !completed_one && i == 0) {
            // nothing completed at all: keep first move as fallback
            best = ml.m[0];
            break;
        }
        if (stop_) break;
        completed_one = true;
        if (s > best_score) {
            best_score = s;
            best = ml.m[i];
            if (s > alpha) alpha = s;
        }
        if (nodes_ >= lim.max_nodes) break;
    }
    r.best = best;
    r.score = best_score == -INF ? 0 : best_score;
    r.nodes = nodes_;
    r.depth = depth;
    return r;
}
