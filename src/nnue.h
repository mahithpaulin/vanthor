// VANTHOR nnue.h — HalfKP inference with incremental updates.
#pragma once
#include "position.h"
#include <cstdint>
#include <string>
#include <vector>

// Feature layout (must match tools/train_nnue.py exactly):
//   white perspective: idx = wk*641 + slot*64 + sq,
//     slot = (color==WHITE ? 0:1)*5 + pt (pt: P=0 N=1 B=2 R=3 Q=4, no kings)
//   black perspective: idx = (bk^56)*641 + slot*64 + (sq^56),
//     slot = (color==BLACK ? 0:1)*5 + pt
constexpr int NNUE_INPUT = 41024;

struct NnueWeights {
    int H = 0;
    std::vector<float> ft;       // [41024*H]
    std::vector<float> ft_bias;  // [H]
    std::vector<float> out_w;    // [H] applied to side-to-move half
    std::vector<float> out_b;    // [H] applied to other half
    float out_bias = 0;
    bool loaded = false;
    bool load(const std::string& path);  // VNTH1 float32 format
};

class NnueState {
public:
    void set_weights(const NnueWeights* w) { w_ = w; }
    bool ready() const { return w_ && w_->loaded; }
    // full recompute from a position (init + king moves + verification)
    void full_refresh(const Position& pos);
    // incremental update; call AFTER pos.do_move(m). captured_pt: piece type
    // captured (NO_PIECE if none); cap_sq: square removed (to, or ep victim).
    void apply(const Position& post, Move m, Color us, int captured_pt, int cap_sq);
    void revert();  // must pair LIFO with apply()
    void apply_null() {  // null move changes no features; keeps stack aligned
        if (!ready()) return;
        Ply ply;
        ply.start = changes_.size();
        stack_.push_back(std::move(ply));
    }
    int evaluate(Color stm) const;  // centipawns, stm-relative
    // verification: recompute both halves from scratch and compare
    bool verify_acc(const Position& pos, float tol) const;

private:
    const NnueWeights* w_ = nullptr;
    std::vector<float> acc_w_, acc_b_;
    struct Change { uint8_t half; uint32_t idx; int8_t dir; };
    struct Ply {
        size_t start = 0;
        bool ref_w = false, ref_b = false;
        std::vector<float> saved_w, saved_b;
    };
    std::vector<Change> changes_;
    std::vector<Ply> stack_;

    int feat_idx(int half, int king_sq, Color color, int pt, int sq) const;
    void add_feat(int half, int idx, int dir);
    void refresh_half(const Position& pos, int half);
};
