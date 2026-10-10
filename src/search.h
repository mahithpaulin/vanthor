// VANTHOR search.h — v0.2.0: ID + aspiration + PVS + TT + LMR/NMP/RFP + SMP.
#pragma once
#include "position.h"
#include "movegen.h"
#include "evaluate.h"
#include "nnue.h"
#include "tt.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

struct SearchOptions {
    bool nullmove = true;
    bool lmr = true;
    bool rfp = true;
    bool checkext = true;
};

struct SearchLimits {
    int depth = 64;
    uint64_t max_nodes = UINT64_MAX;
    int64_t soft_ms = -1;   // stop starting new iterations after this
    int64_t hard_ms = -1;   // abort search unconditionally at this
    bool infinite = false;
};

struct SharedState {
    TT* tt = nullptr;
    const NnueWeights* weights = nullptr;  // null = HCE only
    bool use_nnue = true;                  // UseNNUE option (default on)
    // Shared move-history (butterfly), accumulated across searches and
    // optionally persisted to disk: the engine's long-term learning.
    // Workers snapshot it at iteration start and merge back at the end.
    int hist[2][64][64] = {};
    std::mutex hist_m;
    static constexpr int HIST_CAP = (1 << 28);
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> nodes{0};
    std::chrono::steady_clock::time_point hard_deadline;
    bool has_hard = false;
    std::mutex best_m;
    int best_depth = 0;
    Score best_score = 0;
    Move best_move = NO_MOVE;
    SearchOptions opts;
    SearchLimits lim;
    std::chrono::steady_clock::time_point t0;
};

struct InfoLine {
    int depth = 0;
    Score score = 0;
    uint64_t nodes = 0;
    int64_t ms = 0;
    std::vector<Move> pv;
};

// One search thread. Main thread (tid 0) prints; helpers run silently.
class Worker {
public:
    Worker(SharedState* sh, int tid) : sh_(sh), tid_(tid) { clear_tables(); }
    void clear_tables();
    // main-thread iterative deepening with aspiration + info output
    Move iterate(const Position& root, std::function<void(const InfoLine&)> info);
    // helper-thread fixed-depth run (slight depth offset for diversity)
    void helper_run(const Position& root, int extra_depth);

private:
    SharedState* sh_;
    int tid_;
    Position pos_;
    NnueState nn_;
    bool use_nn_ = false;
    Move killers_[128][2];
    int history_[2][64][64];

    void poll();
    void snapshot_history();  // shared -> local at search start
    void merge_history();     // local -> shared at search end
    int eval_state() {  // NNUE when available+enabled, else HCE
        return (use_nn_ && nn_.ready()) ? nn_.evaluate(pos_.side()) : evaluate(pos_);
    }
    void do_move_nn(Move m);
    void undo_move_nn(Move m);
    void do_null_nn();
    void undo_null_nn();
    int search(int depth, int alpha, int beta, int ply, bool pv, bool last_null);
    int quiescence(int alpha, int beta, int ply, bool pv);
    int search_root(const Position& root, MoveList& ml, int depth, int alpha, int beta,
                    Move& best, std::vector<Move>* pv_out);
    void order_moves(MoveList& ml, int* scores, Move tt_move, int ply);
    void extract_pv(const Position& root, Move first, int depth, std::vector<Move>& pv);
};

// Owns the TT + thread pool for one `go`.
class Search {
public:
    Search() { sh_.tt = &tt_; }
    TT& tt() { return tt_; }
    NnueWeights& net() { return net_; }
    void set_use_nnue(bool b) { sh_.use_nnue = b; }
    // Persistent learning: file format magic "VNTHHIST1" + int32[2][64][64].
    bool load_history(const std::string& path);
    bool save_history(const std::string& path);
    // Runs synchronously; calls info() from the main search thread as
    // iterations complete. Returns when done or stopped.
    Move go(const Position& root, const SearchLimits& lim, int threads,
            const SearchOptions& opts, std::function<void(const InfoLine&)> info,
            uint64_t* out_nodes);
    void stop() { sh_.stop.store(true, std::memory_order_relaxed); }
    bool stopped() const { return sh_.stop.load(std::memory_order_relaxed); }
    uint64_t nodes() const { return sh_.nodes.load(std::memory_order_relaxed); }

private:
    TT tt_;
    NnueWeights net_;
    SharedState sh_;
};
