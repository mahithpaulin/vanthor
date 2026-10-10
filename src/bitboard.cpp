// VANTHOR bitboard.cpp — attack generation with runtime magic search.
// Original implementation for Project Vanthor. Magic numbers are found at
// startup by a deterministic search (fixed seed), so no external tables.
#include "bitboard.h"

namespace {
    // splitmix64 — deterministic PRNG for magic search
    struct Rng {
        uint64_t s;
        explicit Rng(uint64_t seed) : s(seed) {}
        uint64_t next() {
            uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
        uint64_t sparse() { return next() & next() & next(); }
    };

    Bitboard KNIGHT_T[64], KING_T[64], PAWN_T[2][64];

    struct Magic { Bitboard mask = 0; Bitboard magic = 0; int shift = 0; };
    Magic MAG_B[64], MAG_R[64];
    Bitboard ATK_B[64][512], ATK_R[64][4096];

    // direction list: (df, dr)
    constexpr int DIRS_B[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    constexpr int DIRS_R[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

    Bitboard slide_mask(int sq, const int dirs[4][2]) {
        int f = file_of(sq), r = rank_of(sq);
        Bitboard m = 0;
        for (int d = 0; d < 4; d++) {
            int cf = f + dirs[d][0], cr = r + dirs[d][1];
            // exclude edge squares from the mask
            while (cf >= 0 && cf < 8 && cr >= 0 && cr < 8) {
                int nf = cf + dirs[d][0], nr = cr + dirs[d][1];
                if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
                m |= 1ULL << make_sq(cf, cr);
                cf = nf; cr = nr;
            }
        }
        return m;
    }

    Bitboard slide_attack(int sq, Bitboard occ, const int dirs[4][2]) {
        int f = file_of(sq), r = rank_of(sq);
        Bitboard a = 0;
        for (int d = 0; d < 4; d++) {
            int cf = f + dirs[d][0], cr = r + dirs[d][1];
            while (cf >= 0 && cf < 8 && cr >= 0 && cr < 8) {
                int t = make_sq(cf, cr);
                a |= 1ULL << t;
                if (occ & (1ULL << t)) break;
                cf += dirs[d][0]; cr += dirs[d][1];
            }
        }
        return a;
    }

    Bitboard find_magic(int sq, bool bishop) {
        const int (*dirs)[2] = bishop ? DIRS_B : DIRS_R;
        Bitboard mask = slide_mask(sq, dirs);
        int bits = popcount(mask);
        int shift = 64 - bits;
        int n = 1 << bits;
        static Bitboard occ[4096], atk[4096];
        for (int i = 0; i < n; i++) {
            occ[i] = 0;
            Bitboard m = mask;
            // i-th subset of mask
            for (int b = 0; b < bits; b++) {
                int s = poplsb(m);
                if (i & (1 << b)) occ[i] |= 1ULL << s;
            }
            atk[i] = slide_attack(sq, occ[i], dirs);
        }
        Rng rng(bishop ? 0x123456789ABCULL + (uint64_t)sq
                       : 0xFEDCBA987654ULL + (uint64_t)sq * 0x9E37ULL);
        static Bitboard used[4096];
        static int used_epoch[4096];
        static int epoch = 0;
        for (int tries = 0; tries < 1000000; tries++) {
            Bitboard magic = rng.sparse();
            // require the top bits of (mask*magic) to be reasonably dense
            if (popcount((mask * magic) & 0xFF00000000000000ULL) < 6) continue;
            ++epoch;
            bool fail = false;
            for (int i = 0; i < n && !fail; i++) {
                int idx = int((occ[i] * magic) >> shift);
                if (used_epoch[idx] == epoch) {
                    if (used[idx] != atk[i]) fail = true;
                } else {
                    used_epoch[idx] = epoch;
                    used[idx] = atk[i];
                }
            }
            if (!fail) return magic;
        }
        return 0;  // unreachable in practice
    }
}

int popcount(Bitboard b) { return __builtin_popcountll(b); }
int lsb(Bitboard b) { return __builtin_ctzll(b); }
int poplsb(Bitboard& b) { int s = lsb(b); b &= b - 1; return s; }

namespace Attacks {
    void init() {
        const int KN[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
        const int KI[8][2] = {{1,1},{1,0},{1,-1},{0,1},{0,-1},{-1,1},{-1,0},{-1,-1}};
        for (int sq = 0; sq < 64; sq++) {
            int f = file_of(sq), r = rank_of(sq);
            Bitboard kn = 0, ki = 0;
            for (auto& d : KN) {
                int cf = f + d[0], cr = r + d[1];
                if (cf >= 0 && cf < 8 && cr >= 0 && cr < 8) kn |= 1ULL << make_sq(cf, cr);
            }
            for (auto& d : KI) {
                int cf = f + d[0], cr = r + d[1];
                if (cf >= 0 && cf < 8 && cr >= 0 && cr < 8) ki |= 1ULL << make_sq(cf, cr);
            }
            KNIGHT_T[sq] = kn;
            KING_T[sq] = ki;
            // pawns attack "up" the board from their own perspective
            PAWN_T[WHITE][sq] = 0;
            PAWN_T[BLACK][sq] = 0;
            if (r < 7) {
                if (f > 0) PAWN_T[WHITE][sq] |= 1ULL << make_sq(f - 1, r + 1);
                if (f < 7) PAWN_T[WHITE][sq] |= 1ULL << make_sq(f + 1, r + 1);
            }
            if (r > 0) {
                if (f > 0) PAWN_T[BLACK][sq] |= 1ULL << make_sq(f - 1, r - 1);
                if (f < 7) PAWN_T[BLACK][sq] |= 1ULL << make_sq(f + 1, r - 1);
            }
        }
        for (int sq = 0; sq < 64; sq++) {
            MAG_B[sq].mask = slide_mask(sq, DIRS_B);
            MAG_B[sq].magic = find_magic(sq, true);
            MAG_B[sq].shift = 64 - popcount(MAG_B[sq].mask);
            int n = 1 << popcount(MAG_B[sq].mask);
            for (int i = 0; i < n; i++) {
                Bitboard occ = 0;
                Bitboard m = MAG_B[sq].mask;
                int bits = popcount(m);
                for (int b = 0; b < bits; b++) {
                    int s = poplsb(m);
                    if (i & (1 << b)) occ |= 1ULL << s;
                }
                int idx = int((occ * MAG_B[sq].magic) >> MAG_B[sq].shift);
                ATK_B[sq][idx] = slide_attack(sq, occ, DIRS_B);
            }
            MAG_R[sq].mask = slide_mask(sq, DIRS_R);
            MAG_R[sq].magic = find_magic(sq, false);
            MAG_R[sq].shift = 64 - popcount(MAG_R[sq].mask);
            n = 1 << popcount(MAG_R[sq].mask);
            for (int i = 0; i < n; i++) {
                Bitboard occ = 0;
                Bitboard m = MAG_R[sq].mask;
                int bits = popcount(m);
                for (int b = 0; b < bits; b++) {
                    int s = poplsb(m);
                    if (i & (1 << b)) occ |= 1ULL << s;
                }
                int idx = int((occ * MAG_R[sq].magic) >> MAG_R[sq].shift);
                ATK_R[sq][idx] = slide_attack(sq, occ, DIRS_R);
            }
        }
    }

    Bitboard knight(int sq) { return KNIGHT_T[sq]; }
    Bitboard king(int sq) { return KING_T[sq]; }
    Bitboard pawn(Color c, int sq) { return PAWN_T[c][sq]; }
    Bitboard bishop(int sq, Bitboard occ) {
        const Magic& mg = MAG_B[sq];
        return ATK_B[sq][((occ & mg.mask) * mg.magic) >> mg.shift];
    }
    Bitboard rook(int sq, Bitboard occ) {
        const Magic& mg = MAG_R[sq];
        return ATK_R[sq][((occ & mg.mask) * mg.magic) >> mg.shift];
    }
    Bitboard queen(int sq, Bitboard occ) { return bishop(sq, occ) | rook(sq, occ); }

    Bitboard between(int a, int b) {
        int fa = file_of(a), ra = rank_of(a), fb = file_of(b), rb = rank_of(b);
        int df = (fb > fa) ? 1 : (fb < fa) ? -1 : 0;
        int dr = (rb > ra) ? 1 : (rb < ra) ? -1 : 0;
        if (df != 0 && dr != 0 && (fb - fa) * dr != (rb - ra) * df) return 0;  // not diagonal
        if (df == 0 && dr == 0) return 0;
        if (df != 0 && dr == 0 && ra != rb) return 0;
        if (df == 0 && dr != 0 && fa != fb) return 0;
        Bitboard bb = 0;
        int cf = fa + df, cr = ra + dr;
        while (cf != fb || cr != rb) {
            bb |= 1ULL << make_sq(cf, cr);
            cf += df; cr += dr;
        }
        return bb;
    }

    Bitboard ray(int sq, int delta) {
        int df = 0, dr = 0;
        switch (delta) {
            case 1: df = 1; break; case -1: df = -1; break;
            case 8: dr = 1; break; case -8: dr = -1; break;
            case 9: df = 1; dr = 1; break; case 7: df = -1; dr = 1; break;
            case -7: df = 1; dr = -1; break; case -9: df = -1; dr = -1; break;
            default: return 0;
        }
        Bitboard bb = 0;
        int cf = file_of(sq) + df, cr = rank_of(sq) + dr;
        while (cf >= 0 && cf < 8 && cr >= 0 && cr < 8) {
            bb |= 1ULL << make_sq(cf, cr);
            cf += df; cr += dr;
        }
        return bb;
    }
}
