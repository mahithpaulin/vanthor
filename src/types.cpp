// VANTHOR types.cpp
#include "types.h"

std::string sq_name(int sq) {
    std::string s;
    s += char('a' + file_of(sq));
    s += char('1' + rank_of(sq));
    return s;
}

std::string move_name(Move m) {
    if (m == NO_MOVE) return "0000";
    std::string s = sq_name(move_from(m)) + sq_name(move_to(m));
    if (move_is_promo(m)) {
        static const char pc[7] = {'?', 'n', 'b', 'r', 'q', '?', '?'};
        s += pc[move_promo(m)];
    }
    return s;
}
