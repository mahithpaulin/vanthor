// VANTHOR tt.cpp — lock-free TT. Sequence-lock protocol: writer stores
// data then key(release); reader loads key, data, key and accepts only on
// full-key match. A torn data word with a matching key is theoretically
// possible but only perturbs search bounds, never correctness of play
// (all moves played are legally generated).
#include "tt.h"

uint64_t TT::pack(Move m, Score s, uint8_t d, TTFlag f) {
    uint16_t sc = (uint16_t)(int16_t)(s < -32768 ? -32768 : (s > 32767 ? 32767 : s));
    return (uint64_t)m | ((uint64_t)sc << 32) | ((uint64_t)d << 48) | ((uint64_t)f << 56);
}

void TT::unpack(uint64_t data, Move& m, Score& s, uint8_t& d, TTFlag& f) {
    m = Move(data & 0xFFFFFFFFULL);
    s = (Score)(int16_t)((data >> 32) & 0xFFFFULL);
    d = (uint8_t)((data >> 48) & 0xFFULL);
    f = (TTFlag)((data >> 56) & 0xFFULL);
}

void TT::resize(int megabytes) {
    if (megabytes < 1) megabytes = 1;
    if (megabytes > 1024) megabytes = 1024;
    size_t want = (size_t)megabytes * 1024 * 1024 / sizeof(TTSlot);
    size_t n = 1;
    while (n * 2 <= want) n *= 2;
    table_.reset(new TTSlot[n]());
    size_ = n;
    mask_ = n - 1;
    clear();
}

void TT::clear() {
    for (size_t i = 0; i < size_; i++) {
        table_[i].key.store(0, std::memory_order_relaxed);
        table_[i].data.store(0, std::memory_order_relaxed);
    }
}

TTEntry TT::probe(Key key) {
    TTEntry e;
    if (!table_ || key == 0) return e;
    TTSlot& slot = table_[key & mask_];
    Key k1 = slot.key.load(std::memory_order_acquire);
    uint64_t data = slot.data.load(std::memory_order_relaxed);
    Key k2 = slot.key.load(std::memory_order_acquire);
    if (k1 != key || k2 != key || data == 0) return e;
    unpack(data, e.move, e.score, e.depth, e.flag);
    if (e.flag == TT_NONE || e.flag > TT_UPPER) return e;
    e.hit = true;
    return e;
}

void TT::store(Key key, Move move, Score score, uint8_t depth, TTFlag flag) {
    if (!table_ || key == 0 || move == NO_MOVE) return;
    TTSlot& slot = table_[key & mask_];
    // depth-preferred replacement with always-replace fallback for shallow
    uint64_t old = slot.data.load(std::memory_order_relaxed);
    Move om; Score os; uint8_t od; TTFlag of;
    unpack(old, om, os, od, of);
    Key ok = slot.key.load(std::memory_order_relaxed);
    if (ok != 0 && od > depth && od - depth > 3) return;  // keep deeper entry
    slot.data.store(pack(move, score, depth, flag), std::memory_order_relaxed);
    slot.key.store(key, std::memory_order_release);
}
