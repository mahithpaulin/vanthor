// VANTHOR search.cpp — v0.2.0. Original implementation.
#include "search.h"
#include "movegen.h"
#include "evaluate.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    const int MAX_PLY = 96;
    const int VAL[7] = {100, 320, 330, 500, 900, 20000, 0};
    const int RFP_MARGIN[4] = {0, 150, 300, 450};

    int LMR_T[64][64];
    bool LMR_INIT = false;
    void init_lmr() {
        if (LMR_INIT) return;
        for (int d = 0; d < 64; d++)
            for (int m = 0; m < 64; m++)
                LMR_T[d][m] = (d > 2 && m > 2) ? (int)(log((double)d) * log((double)m) / 2.0) : 0;
        LMR_INIT = true;
    }

    inline Score tt_to_search(Score s, int ply) {
        if (s > MATE - 1000) return s - ply;
        if (s < -MATE + 1000) return s + ply;
        return s;
    }
    inline Score search_to_tt(Score s, int ply) {
        if (s > MATE - 1000) return s + ply;
        if (s < -MATE + 1000) return s - ply;
        return s;
    }
}

void Worker::clear_tables() {
    memset(killers_, 0, sizeof(killers_));
    memset(history_, 0, sizeof(history_));
}

void Worker::poll() {
    if (sh_->stop.load(std::memory_order_relaxed)) return;
    if (sh_->nodes.load(std::memory_order_relaxed) >= sh_->lim.max_nodes) {
        sh_->stop.store(true, std::memory_order_relaxed);
        return;
    }
    if (sh_->has_hard && std::chrono::steady_clock::now() >= sh_->hard_deadline)
        sh_->stop.store(true, std::memory_order_relaxed);
}

void Worker::order_moves(MoveList& ml, int* scores, Move tt_move, int ply) {
    Color us = pos_.side();
    (void)us;
    for (int i = 0; i < ml.n; i++) {
        Move m = ml.m[i];
        int s = 0;
        if (m == tt_move) s = 3000000;
        else if (move_is_promo(m)) s = 2000000 + int(move_promo(m)) * 10000;
        else if (move_is_capture(m)) {
            int to = move_to(m), from = move_from(m);
            int victim = move_is_ep(m) ? PAWN : (pos_.piece_on(to) % 6);
            int attacker = pos_.piece_on(from) % 6;
            int base = VAL[victim] * 16 - VAL[attacker];
            s = (see_capture(pos_, m) >= 0 ? 1000000 : 100000) + base;
        } else if (m == killers_[ply][0]) s = 900000;
        else if (m == killers_[ply][1]) s = 899000;
        else {
            int h = history_[pos_.side()][move_from(m)][move_to(m)];
            s = h > 800000 ? 800000 : h;
        }
        scores[i] = s;
    }
}

int Worker::quiescence(int alpha, int beta, int ply, bool pv) {
    (void)pv;
    if ((sh_->nodes.fetch_add(1, std::memory_order_relaxed) & 1023) == 0) {
        poll();
        if (sh_->stop.load(std::memory_order_relaxed)) return alpha;
    }
    if (pos_.half() >= 100 || pos_.repetitions() >= 2) return DRAW_SCORE;
    if (ply >= MAX_PLY) return evaluate(pos_);

    Key key = pos_.key();
    TTEntry tte = sh_->tt->probe(key);
    Move tt_move = tte.hit ? tte.move : NO_MOVE;
    if (tte.hit) {  // any stored depth dominates a qsearch node
        Score s = tt_to_search(tte.score, ply);
        if (tte.flag == TT_EXACT) return s;
        if (tte.flag == TT_LOWER && s >= beta) return s;
        if (tte.flag == TT_UPPER && s <= alpha) return s;
    }

    bool check = pos_.in_check(pos_.side());
    int stand = check ? -INF : evaluate(pos_);
    if (!check) {
        if (stand >= beta) {
            sh_->tt->store(key, tt_move == NO_MOVE ? NO_MOVE : tt_move, search_to_tt(beta, ply), 0, TT_LOWER);
            return beta;
        }
        if (stand > alpha) alpha = stand;
    }
    MoveList ml;
    if (check) generate_legal(pos_, ml);
    else generate_legal_captures(pos_, ml);
    if (check && ml.n == 0) return -MATE + ply;

    int scores[256];
    order_moves(ml, scores, tt_move, ply < 128 ? ply : 127);
    int best = check ? -INF : stand;
    Move best_move = NO_MOVE;
    int alpha_orig = alpha;
    for (int i = 0; i < ml.n; i++) {
        int bi = i;
        for (int j = i + 1; j < ml.n; j++)
            if (scores[j] > scores[bi]) bi = j;
        std::swap(ml.m[i], ml.m[bi]);
        std::swap(scores[i], scores[bi]);
        if (!check && see_capture(pos_, ml.m[i]) < 0) continue;
        pos_.do_move(ml.m[i]);
        int s = -quiescence(-beta, -alpha, ply + 1, pv);
        pos_.undo_move(ml.m[i]);
        if (sh_->stop.load(std::memory_order_relaxed)) return alpha;
        if (s > best) {
            best = s;
            best_move = ml.m[i];
            if (s > alpha) {
                alpha = s;
                if (alpha >= beta) break;
            }
        }
    }
    TTFlag flag = (best >= beta) ? TT_LOWER : (best > alpha_orig ? TT_EXACT : TT_UPPER);
    if (best_move != NO_MOVE)
        sh_->tt->store(key, best_move, search_to_tt(best, ply), 0, flag);
    return best;
}

int Worker::search(int depth, int alpha, int beta, int ply, bool pv, bool last_null) {
    if ((sh_->nodes.fetch_add(1, std::memory_order_relaxed) & 1023) == 0) {
        poll();
        if (sh_->stop.load(std::memory_order_relaxed)) return alpha;
    }
    if (pos_.half() >= 100 || pos_.repetitions() >= 2) return DRAW_SCORE;
    if (ply >= MAX_PLY) return evaluate(pos_);
    if (depth <= 0) return quiescence(alpha, beta, ply, pv);

    // mate distance pruning
    if (ply > 0) {
        alpha = std::max(alpha, -MATE + ply);
        beta = std::min(beta, MATE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    Color us = pos_.side();
    bool in_check = pos_.in_check(us);
    if (sh_->opts.checkext && in_check) {
        depth++;
        if (depth > 64) depth = 64;
    }

    Key key = pos_.key();
    TTEntry tte = sh_->tt->probe(key);
    Move tt_move = tte.hit ? tte.move : NO_MOVE;
    if (tte.hit && tte.depth >= depth && ply > 0) {
        Score s = tt_to_search(tte.score, ply);
        if (tte.flag == TT_EXACT) return s;
        if (tte.flag == TT_LOWER && s >= beta) return s;
        if (tte.flag == TT_UPPER && s <= alpha) return s;
    }

    int eval = evaluate(pos_);

    // reverse futility pruning
    if (sh_->opts.rfp && !pv && !in_check && depth <= 3) {
        if (eval - RFP_MARGIN[depth] >= beta) return eval;
    }

    // null-move pruning
    if (sh_->opts.nullmove && depth >= 3 && !in_check && !last_null &&
        eval >= beta && pos_.has_non_pawn(us)) {
        pos_.do_null();
        int R = 3 + (depth > 6 ? 1 : 0);
        int s = -search(depth - 1 - R, -beta, -beta + 1, ply + 1, false, true);
        pos_.undo_null();
        if (sh_->stop.load(std::memory_order_relaxed)) return 0;
        if (s >= beta) return (s > MATE - 1000) ? beta : s;
    }

    MoveList ml;
    generate_legal(pos_, ml);
    if (ml.n == 0) return in_check ? -MATE + ply : DRAW_SCORE;

    int scores[256];
    order_moves(ml, scores, tt_move, ply < 128 ? ply : 127);
    int best = -INF;
    Move best_move = NO_MOVE;
    int alpha_orig = alpha;
    int moves_done = 0;
    for (int i = 0; i < ml.n; i++) {
        int bi = i;
        for (int j = i + 1; j < ml.n; j++)
            if (scores[j] > scores[bi]) bi = j;
        std::swap(ml.m[i], ml.m[bi]);
        std::swap(scores[i], scores[bi]);
        Move m = ml.m[i];
        bool capture = move_is_capture(m);
        bool promo = move_is_promo(m);
        pos_.do_move(m);
        int s;
        if (moves_done == 0) {
            s = -search(depth - 1, -beta, -alpha, ply + 1, pv, false);
        } else {
            // late-move reduction
            int r = 0;
            if (sh_->opts.lmr && depth >= 3 && moves_done >= 3 && !capture &&
                !promo && !in_check && m != killers_[ply][0] && m != killers_[ply][1]) {
                r = LMR_T[std::min(depth, 63)][std::min(moves_done, 63)];
                if (!pv) r++;
                if (r > depth - 1) r = depth - 1;
            }
            if (r > 0) {
                s = -search(depth - 1 - r, -alpha - 1, -alpha, ply + 1, false, false);
                if (sh_->stop.load(std::memory_order_relaxed)) { pos_.undo_move(m); return 0; }
                if (s > alpha)  // research at full depth
                    s = -search(depth - 1, -alpha - 1, -alpha, ply + 1, false, false);
            } else {
                s = -search(depth - 1, -alpha - 1, -alpha, ply + 1, false, false);
            }
            if (!sh_->stop.load(std::memory_order_relaxed) && s > alpha && s < beta)
                s = -search(depth - 1, -beta, -alpha, ply + 1, true, false);
        }
        pos_.undo_move(m);
        if (sh_->stop.load(std::memory_order_relaxed)) return 0;
        moves_done++;
        if (s > best) {
            best = s;
            best_move = m;
            if (s > alpha) {
                alpha = s;
                if (alpha >= beta) {
                    // killer + history update (quiets only)
                    if (!capture && !promo && ply < 128) {
                        if (killers_[ply][0] != m) {
                            killers_[ply][1] = killers_[ply][0];
                            killers_[ply][0] = m;
                        }
                        int& h = history_[us][move_from(m)][move_to(m)];
                        h += depth * depth;
                        if (h > (1 << 28)) {
                            for (int c = 0; c < 2; c++)
                                for (int a = 0; a < 64; a++)
                                    for (int b = 0; b < 64; b++)
                                        history_[c][a][b] >>= 1;
                        }
                    }
                    break;
                }
            }
        }
    }

    TTFlag flag = (best >= beta) ? TT_LOWER : (best > alpha_orig ? TT_EXACT : TT_UPPER);
    if (best_move != NO_MOVE)
        sh_->tt->store(key, best_move, search_to_tt(best, ply), (uint8_t)std::min(depth, 127), flag);
    return best;
}

int Worker::search_root(const Position& root, MoveList& ml, int depth, int alpha,
                        int beta, Move& best, std::vector<Move>* pv_out) {
    pos_ = root;
    int alpha_orig = alpha;
    TTEntry tte = sh_->tt->probe(root.key());
    Move tt_move = tte.hit ? tte.move : NO_MOVE;
    int scores[256];
    order_moves(ml, scores, tt_move, 0);
    int best_score = -INF;
    best = NO_MOVE;
    for (int i = 0; i < ml.n; i++) {
        int bi = i;
        for (int j = i + 1; j < ml.n; j++)
            if (scores[j] > scores[bi]) bi = j;
        std::swap(ml.m[i], ml.m[bi]);
        std::swap(scores[i], scores[bi]);
        pos_.do_move(ml.m[i]);
        int s;
        if (i == 0) s = -search(depth - 1, -beta, -alpha, 1, true, false);
        else {
            s = -search(depth - 1, -alpha - 1, -alpha, 1, false, false);
            if (!sh_->stop.load(std::memory_order_relaxed) && s > alpha && s < beta)
                s = -search(depth - 1, -beta, -alpha, 1, true, false);
        }
        pos_.undo_move(ml.m[i]);
        if (sh_->stop.load(std::memory_order_relaxed)) return best_score;
        if (s > best_score) {
            best_score = s;
            best = ml.m[i];
            if (s > alpha) {
                alpha = s;
                if (alpha >= beta) break;
            }
        }
    }
    if (best != NO_MOVE) {
        TTFlag flag = (best_score >= beta) ? TT_LOWER : (best_score > alpha_orig ? TT_EXACT : TT_UPPER);
        sh_->tt->store(root.key(), best, search_to_tt(best_score, 0),
                       (uint8_t)std::min(depth, 127), flag);
        if (pv_out) extract_pv(root, best, depth, *pv_out);
    }
    return best_score;
}

void Worker::extract_pv(const Position& root, Move first, int depth, std::vector<Move>& pv) {
    pv.clear();
    Position p = root;
    pv.push_back(first);
    p.do_move(first);
    for (int i = 1; i < depth && i < 32; i++) {
        TTEntry t = sh_->tt->probe(p.key());
        if (!t.hit || t.move == NO_MOVE) break;
        MoveList ml;
        generate_legal(p, ml);
        bool ok = false;
        for (int j = 0; j < ml.n; j++)
            if (ml.m[j] == t.move) { ok = true; break; }
        if (!ok) break;
        pv.push_back(t.move);
        p.do_move(t.move);
    }
}

Move Worker::iterate(const Position& root, std::function<void(const InfoLine&)> info) {
    init_lmr();
    MoveList ml;
    generate_legal(root, ml);
    if (ml.n == 0) return NO_MOVE;
    if (ml.n == 1) {
        if (info) {
            InfoLine il;
            il.depth = 1;
            il.pv = {ml.m[0]};
            info(il);
        }
        return ml.m[0];
    }
    int max_depth = sh_->lim.depth;
    if (max_depth > 64) max_depth = 64;
    Move best = NO_MOVE;
    Score prev = 0;
    for (int depth = 1; depth <= max_depth; depth++) {
        int alpha = -INF, beta = INF;
        bool asp = (depth >= 4);
        if (asp) {
            int delta = 25;
            alpha = prev - delta;
            beta = prev + delta;
        }
        Move iter_best = NO_MOVE;
        int s = -INF;
        std::vector<Move> pv;
        while (true) {
            s = search_root(root, ml, depth, alpha, beta, iter_best, &pv);
            if (sh_->stop.load(std::memory_order_relaxed)) break;
            if (!asp) break;
            if (s <= alpha) {
                alpha = std::max(alpha - 150, -INF);
                beta = (alpha + beta) / 2;
            } else if (s >= beta) {
                beta = std::min(beta + 150, INF);
            } else break;
            if (beta - alpha > 600) {  // too wide: full window
                alpha = -INF; beta = INF;
            }
        }
        if (sh_->stop.load(std::memory_order_relaxed)) break;
        // reorder root moves: completed best first next iteration
        for (int i = 0; i < ml.n; i++)
            if (ml.m[i] == iter_best && i > 0) {
                Move t = ml.m[0]; ml.m[0] = ml.m[i]; ml.m[i] = t;
                break;
            }
        best = iter_best;
        prev = s;
        {
            std::lock_guard<std::mutex> lk(sh_->best_m);
            if (depth > sh_->best_depth) {
                sh_->best_depth = depth;
                sh_->best_score = s;
                sh_->best_move = best;
            }
        }
        if (info) {
            InfoLine il;
            il.depth = depth;
            il.score = s;
            il.nodes = sh_->nodes.load(std::memory_order_relaxed);
            il.ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - sh_->t0).count();
            il.pv = pv;
            info(il);
        }
        if (s > MATE - 1000 || s < -MATE + 1000) break;  // found mate
        if (sh_->lim.max_nodes != UINT64_MAX &&
            sh_->nodes.load(std::memory_order_relaxed) >= sh_->lim.max_nodes)
            break;
        if (!sh_->lim.infinite && sh_->lim.soft_ms >= 0) {
            int64_t el = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - sh_->t0).count();
            if (el >= sh_->lim.soft_ms) break;
            // don't start a long iteration when most of the budget is spent
            if (depth >= 6 && el >= sh_->lim.soft_ms / 2) {
                // heuristic only; the hard deadline still guards us
            }
        }
        if (sh_->stop.load(std::memory_order_relaxed)) break;
    }
    return best;
}

void Worker::helper_run(const Position& root, int depth_offset) {
    init_lmr();
    MoveList ml;
    generate_legal(root, ml);
    if (ml.n == 0) return;
    int max_depth = sh_->lim.depth;
    if (max_depth > 64) max_depth = 64;
    Score prev = 0;
    for (int depth = 1 + depth_offset; depth <= max_depth; depth++) {
        int alpha = -INF, beta = INF;
        if (depth >= 4 + depth_offset) {
            alpha = prev - 25;
            beta = prev + 25;
        }
        Move iter_best = NO_MOVE;
        int s = search_root(root, ml, depth, alpha, beta, iter_best, nullptr);
        if (sh_->stop.load(std::memory_order_relaxed)) break;
        prev = s;
        {
            std::lock_guard<std::mutex> lk(sh_->best_m);
            if (depth > sh_->best_depth) {
                sh_->best_depth = depth;
                sh_->best_score = s;
                sh_->best_move = iter_best;
            }
        }
        if (s > MATE - 1000 || s < -MATE + 1000) break;
    }
}

Move Search::go(const Position& root, const SearchLimits& lim, int threads,
                const SearchOptions& opts, std::function<void(const InfoLine&)> info,
                uint64_t* out_nodes) {
    sh_.lim = lim;
    sh_.opts = opts;
    sh_.stop.store(false, std::memory_order_relaxed);
    sh_.nodes.store(0, std::memory_order_relaxed);
    sh_.t0 = std::chrono::steady_clock::now();
    sh_.has_hard = (!lim.infinite && lim.hard_ms >= 0);
    if (sh_.has_hard)
        sh_.hard_deadline = sh_.t0 + std::chrono::milliseconds(lim.hard_ms);
    {
        std::lock_guard<std::mutex> lk(sh_.best_m);
        sh_.best_depth = 0;
        sh_.best_score = 0;
        sh_.best_move = NO_MOVE;
    }
    if (threads < 1) threads = 1;
    if (threads > 8) threads = 8;

    Worker main_w(&sh_, 0);
    std::vector<std::thread> helpers;
    std::vector<Worker*> hw;
    for (int i = 1; i < threads; i++) {
        hw.push_back(new Worker(&sh_, i));
        helpers.emplace_back([w = hw.back(), root, i]() {
            w->helper_run(root, i % 3);
        });
    }
    Move best = main_w.iterate(root, info);
    sh_.stop.store(true, std::memory_order_relaxed);
    for (auto& t : helpers) t.join();
    for (auto* w : hw) delete w;
    if (best == NO_MOVE) {
        std::lock_guard<std::mutex> lk(sh_.best_m);
        best = sh_.best_move;
    }
    if (out_nodes) *out_nodes = sh_.nodes.load(std::memory_order_relaxed);
    return best;
}
