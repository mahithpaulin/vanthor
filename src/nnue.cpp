// VANTHOR nnue.cpp — HalfKP inference. Original implementation.
#include "nnue.h"
#include <cstdio>
#include <cstring>

bool NnueWeights::load(const std::string& path) {
    loaded = false;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[5] = {0};
    if (fread(magic, 1, 5, f) != 5 || memcmp(magic, "VNTH1", 5) != 0) {
        fclose(f);
        return false;
    }
    uint32_t h = 0;
    if (fread(&h, 4, 1, f) != 1 || h < 8 || h > 1024) {
        fclose(f);
        return false;
    }
    H = (int)h;
    ft.resize((size_t)NNUE_INPUT * H);
    ft_bias.resize(H);
    out_w.resize(H);
    out_b.resize(H);
    bool ok = fread(ft.data(), 4, (size_t)NNUE_INPUT * H, f) == (size_t)NNUE_INPUT * H
           && fread(ft_bias.data(), 4, H, f) == (size_t)H
           && fread(out_w.data(), 4, H, f) == (size_t)H
           && fread(out_b.data(), 4, H, f) == (size_t)H
           && fread(&out_bias, 4, 1, f) == 1;
    // exact EOF expected (no trailing bytes)
    int c = fgetc(f);
    fclose(f);
    if (!ok || c != EOF) {
        H = 0;
        return false;
    }
    loaded = true;
    return true;
}

int NnueState::feat_idx(int half, int king_sq, Color color, int pt, int sq) const {
    int k = (half == 0) ? king_sq : (king_sq ^ 56);
    int s = (half == 0) ? sq : (sq ^ 56);
    int base = (half == 0) ? (color == WHITE ? 0 : 1) : (color == BLACK ? 0 : 1);
    return k * 641 + (base * 5 + pt) * 64 + s;
}

void NnueState::add_feat(int half, int idx, int dir) {
    std::vector<float>& acc = (half == 0) ? acc_w_ : acc_b_;
    const float* base = w_->ft.data() + (size_t)idx * w_->H;
    if (dir > 0) {
        for (int i = 0; i < w_->H; i++) acc[i] += base[i];
    } else {
        for (int i = 0; i < w_->H; i++) acc[i] -= base[i];
    }
    changes_.push_back(Change{(uint8_t)half, (uint32_t)idx, (int8_t)dir});
}

void NnueState::refresh_half(const Position& pos, int half) {
    std::vector<float>& acc = (half == 0) ? acc_w_ : acc_b_;
    acc.assign(w_->H, 0.0f);
    for (int i = 0; i < w_->H; i++) acc[i] = w_->ft_bias[i];
    int king = (half == 0) ? pos.king_sq(WHITE) : pos.king_sq(BLACK);
    for (int sq = 0; sq < 64; sq++) {
        int v = pos.piece_on(sq);
        if (v < 0 || v % 6 == KING) continue;
        int idx = feat_idx(half, king, Color(v / 6), v % 6, sq);
        const float* base = w_->ft.data() + (size_t)idx * w_->H;
        for (int i = 0; i < w_->H; i++) acc[i] += base[i];
    }
}

void NnueState::full_refresh(const Position& pos) {
    if (!ready()) return;
    acc_w_.assign(w_->H, 0.0f);
    acc_b_.assign(w_->H, 0.0f);
    changes_.clear();
    stack_.clear();
    refresh_half(pos, 0);
    refresh_half(pos, 1);
}

void NnueState::apply(const Position& post, Move m, Color us, int captured_pt, int cap_sq) {
    if (!ready()) return;
    Ply ply;
    ply.start = changes_.size();
    int from = move_from(m), to = move_to(m);
    int vpc = post.piece_on(to);
    int pc = (vpc >= 0) ? vpc % 6 : NO_PIECE;  // piece now on `to`
    bool king_move = (pc == KING);
    bool castle = move_is_castle(m);
    Color them = (us == WHITE) ? BLACK : WHITE;
    int my_half = (us == WHITE) ? 0 : 1;
    int op_half = 1 - my_half;
    int wk = post.king_sq(WHITE), bk = post.king_sq(BLACK);

    if (king_move) {
        // mover's perspective: full refresh (king index changed)
        ply.ref_w = (my_half == 0);
        ply.ref_b = (my_half == 1);
        if (my_half == 0) { ply.saved_w = acc_w_; refresh_half(post, 0); }
        else { ply.saved_b = acc_b_; refresh_half(post, 1); }
        if (castle) {
            // rook from/to known by king destination
            int rf = -1, rt = -1;
            if (to == G1) { rf = H1; rt = F1; }
            else if (to == C1) { rf = A1; rt = D1; }
            else if (to == G8) { rf = H8; rt = F8; }
            else if (to == C8) { rf = A8; rt = D8; }
            if (rf >= 0) {
                int k_opp = (op_half == 0) ? wk : bk;
                add_feat(op_half, feat_idx(op_half, k_opp, us, ROOK, rf), -1);
                add_feat(op_half, feat_idx(op_half, k_opp, us, ROOK, rt), +1);
            }
            int k_my = (my_half == 0) ? wk : bk;
            (void)k_my;  // refreshed half already includes the rook
        } else if (captured_pt != NO_PIECE) {
            // king capture: remove victim from opponent's half
            int k_opp = (op_half == 0) ? wk : bk;
            add_feat(op_half, feat_idx(op_half, k_opp, them, captured_pt, cap_sq), -1);
        }
    } else {
        int moved_pt = move_is_promo(m) ? PAWN : pc;
        int placed_pt = move_is_promo(m) ? move_promo(m) : pc;
        for (int h = 0; h < 2; h++) {
            int k = (h == 0) ? wk : bk;
            add_feat(h, feat_idx(h, k, us, moved_pt, from), -1);
            add_feat(h, feat_idx(h, k, us, placed_pt, to), +1);
            if (captured_pt != NO_PIECE)
                add_feat(h, feat_idx(h, k, them, captured_pt, cap_sq), -1);
        }
    }
    stack_.push_back(std::move(ply));
}

void NnueState::revert() {
    if (!ready() || stack_.empty()) return;
    Ply ply = std::move(stack_.back());
    stack_.pop_back();
    while (changes_.size() > ply.start) {
        Change c = changes_.back();
        changes_.pop_back();
        std::vector<float>& acc = (c.half == 0) ? acc_w_ : acc_b_;
        const float* base = w_->ft.data() + (size_t)c.idx * w_->H;
        if (c.dir > 0) {
            for (int i = 0; i < w_->H; i++) acc[i] -= base[i];
        } else {
            for (int i = 0; i < w_->H; i++) acc[i] += base[i];
        }
    }
    if (ply.ref_w) acc_w_ = std::move(ply.saved_w);
    if (ply.ref_b) acc_b_ = std::move(ply.saved_b);
}

static inline float clipf(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

int NnueState::evaluate(Color stm) const {
    if (!ready()) return 0;
    const float* first = (stm == WHITE) ? acc_w_.data() : acc_b_.data();
    const float* second = (stm == WHITE) ? acc_b_.data() : acc_w_.data();
    double s = w_->out_bias;
    for (int i = 0; i < w_->H; i++)
        s += (double)clipf(first[i]) * w_->out_w[i]
           + (double)clipf(second[i]) * w_->out_b[i];
    return (int)(s * 1000.0);
}

bool NnueState::verify_acc(const Position& pos, float tol) const {
    if (!ready()) return false;
    for (int h = 0; h < 2; h++) {
        std::vector<float> acc(w_->H, 0.0f);
        for (int i = 0; i < w_->H; i++) acc[i] = w_->ft_bias[i];
        int king = (h == 0) ? pos.king_sq(WHITE) : pos.king_sq(BLACK);
        for (int sq = 0; sq < 64; sq++) {
            int v = pos.piece_on(sq);
            if (v < 0 || v % 6 == KING) continue;
            int idx = feat_idx(h, king, Color(v / 6), v % 6, sq);
            const float* base = w_->ft.data() + (size_t)idx * w_->H;
            for (int i = 0; i < w_->H; i++) acc[i] += base[i];
        }
        const std::vector<float>& mine = (h == 0) ? acc_w_ : acc_b_;
        for (int i = 0; i < w_->H; i++) {
            float d = mine[i] - acc[i];
            if (d < -tol || d > tol) return false;
        }
    }
    return true;
}
