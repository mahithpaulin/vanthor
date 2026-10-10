// VANTHOR uci.cpp — protocol loop. Original implementation.
#include "uci.h"
#include "version.h"
#include "movegen.h"
#include "evaluate.h"
#include "perft.h"
#include <iostream>
#include <sstream>
#include <chrono>

Uci::Uci() = default;

void Uci::cmd_uci() {
    std::cout << "id name " << VANTHOR_NAME << " " << VANTHOR_VERSION << "\n";
    std::cout << "id author Project Vanthor\n";
    std::cout << "option name Hash type spin default 16 min 1 max 1024\n";
    std::cout << "option name Threads type spin default 1 min 1 max 8\n";
    std::cout << "uciok\n";
}

bool Uci::play_uci_moves(const std::string& moves) {
    std::istringstream in(moves);
    std::string tok;
    while (in >> tok) {
        if (tok.size() < 4 || tok.size() > 5) return false;
        int from = make_sq(tok[0] - 'a', tok[1] - '1');
        int to = make_sq(tok[2] - 'a', tok[3] - '1');
        int promo = 0;
        if (tok.size() == 5) {
            promo = (tok[4] == 'n') ? KNIGHT : (tok[4] == 'b') ? BISHOP
                  : (tok[4] == 'r') ? ROOK : (tok[4] == 'q') ? QUEEN : -1;
            if (promo < 0) return false;
        }
        MoveList ml;
        generate_legal(pos_, ml);
        bool found = false;
        for (int i = 0; i < ml.n; i++) {
            if (move_from(ml.m[i]) == from && move_to(ml.m[i]) == to &&
                move_promo(ml.m[i]) == promo) {
                pos_.do_move(ml.m[i]);
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void Uci::cmd_position(const std::string& args) {
    std::istringstream in(args);
    std::string tok;
    in >> tok;
    std::string rest, moves;
    std::getline(in, rest);
    if (tok == "startpos") {
        pos_.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::string::size_type p = rest.find("moves");
        if (p != std::string::npos) moves = rest.substr(p + 5);
    } else if (tok == "fen") {
        // rest = " <fen 6 fields> [moves ...]"
        std::string::size_type p = rest.find(" moves ");
        std::string f = (p == std::string::npos) ? rest : rest.substr(0, p);
        if (p != std::string::npos) moves = rest.substr(p + 7);
        while (!f.empty() && f[0] == ' ') f.erase(0, 1);
        if (!pos_.set_fen(f)) {
            std::cout << "info string error: bad FEN\n";
            return;
        }
    } else return;
    if (!moves.empty() && !play_uci_moves(moves))
        std::cout << "info string error: illegal move in list\n";
}

void Uci::cmd_go(const std::string& args) {
    SearchLimits lim;
    lim.depth = 64;
    int64_t movetime = -1, wtime = -1, btime = -1, winc = 0, binc = 0;
    int movestogo = 0;
    bool have_depth = false;
    std::istringstream in(args);
    std::string tok;
    while (in >> tok) {
        if (tok == "depth" && (in >> tok)) { lim.depth = std::stoi(tok); have_depth = true; }
        else if (tok == "movetime" && (in >> tok)) movetime = std::stoll(tok);
        else if (tok == "nodes" && (in >> tok)) lim.max_nodes = (uint64_t)std::stoull(tok);
        else if (tok == "wtime" && (in >> tok)) wtime = std::stoll(tok);
        else if (tok == "btime" && (in >> tok)) btime = std::stoll(tok);
        else if (tok == "winc" && (in >> tok)) winc = std::stoll(tok);
        else if (tok == "binc" && (in >> tok)) binc = std::stoll(tok);
        else if (tok == "movestogo" && (in >> tok)) movestogo = std::stoi(tok);
        else if (tok == "infinite") lim.infinite = true;
    }
    if (movetime >= 0) lim.time_ms = movetime;
    else if ((pos_.side() == WHITE ? wtime : btime) >= 0 && !lim.infinite) {
        int64_t left = (pos_.side() == WHITE) ? wtime : btime;
        int64_t inc = (pos_.side() == WHITE) ? winc : binc;
        int64_t alloc;
        if (movestogo > 0) alloc = left / movestogo + inc / 2;
        else alloc = left / 30 + inc / 2;
        if (alloc < 10) alloc = 10;
        if (alloc > left / 2 && left > 100) alloc = left / 2;
        lim.time_ms = alloc;
    }
    if (!have_depth && lim.time_ms < 0 && !lim.infinite && lim.max_nodes == UINT64_MAX) {
        lim.depth = 8;  // bare `go`: keep it bounded
    }
    auto t0 = std::chrono::steady_clock::now();
    SearchResult r = searcher_.go(pos_, lim);
    auto t1 = std::chrono::steady_clock::now();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    uint64_t nps = ms > 0 ? (r.nodes * 1000 / (uint64_t)ms) : r.nodes;
    std::cout << "info depth " << r.depth << " score cp " << r.score
              << " nodes " << r.nodes << " time " << ms << " nps " << nps << "\n";
    std::cout << "bestmove " << move_name(r.best) << "\n";
}

void Uci::cmd_perft(const std::string& args) {
    int depth = 0;
    try { depth = std::stoi(args); } catch (...) { return; }
    if (depth < 0 || depth > 8) { std::cout << "info string perft depth 0..8\n"; return; }
    auto t0 = std::chrono::steady_clock::now();
    uint64_t nodes = perft(pos_, depth);
    auto t1 = std::chrono::steady_clock::now();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    uint64_t nps = ms > 0 ? (nodes * 1000 / (uint64_t)ms) : nodes;
    std::cout << "nodes " << nodes << " time " << ms << " nps " << nps << "\n";
}

void Uci::cmd_divide(const std::string& args) {
    int depth = 0;
    try { depth = std::stoi(args); } catch (...) { return; }
    if (depth < 1 || depth > 7) { std::cout << "info string divide depth 1..7\n"; return; }
    MoveList ml;
    generate_legal(pos_, ml);
    uint64_t total = 0;
    for (int i = 0; i < ml.n; i++) {
        pos_.do_move(ml.m[i]);
        uint64_t n = perft(pos_, depth - 1);
        pos_.undo_move(ml.m[i]);
        total += n;
        std::cout << move_name(ml.m[i]) << " " << n << "\n";
    }
    std::cout << "nodes " << total << "\n";
}

void Uci::cmd_bench() {
    // fixed benchmark: two positions, fixed depths → deterministic node counts
    const char* fens[2] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"};
    const int depths[2] = {11, 9};
    uint64_t total_nodes = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 2; i++) {
        Position p;
        p.set_fen(fens[i]);
        SearchLimits lim;
        lim.depth = depths[i];
        lim.infinite = true;  // no clock: fixed-depth node count is deterministic
        Searcher s;
        SearchResult r = s.go(p, lim);
        total_nodes += r.nodes;
        std::cout << "info string bench pos " << i << " depth " << depths[i]
                  << " nodes " << r.nodes << " bestmove " << move_name(r.best) << "\n";
    }
    auto t1 = std::chrono::steady_clock::now();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    uint64_t nps = ms > 0 ? (total_nodes * 1000 / (uint64_t)ms) : total_nodes;
    std::cout << "nodes " << total_nodes << " time " << ms << " nps " << nps << "\n";
}

void Uci::cmd_display() {
    for (int r = 7; r >= 0; r--) {
        std::cout << (r + 1) << " ";
        for (int f = 0; f < 8; f++) {
            int v = pos_.piece_on(make_sq(f, r));
            char ch = '.';
            if (v >= 0) {
                static const char names[6] = {'p','n','b','r','q','k'};
                ch = names[v % 6];
                if (v / 6 == WHITE) ch = char(ch - 32);
            }
            std::cout << ch << " ";
        }
        std::cout << "\n";
    }
    std::cout << "  a b c d e f g h\n";
    std::cout << "fen " << pos_.fen() << "\n";
    std::cout << "key " << pos_.key()
              << (pos_.in_check(pos_.side()) ? " check" : "") << "\n";
}

void Uci::cmd_eval() {
    std::cout << "eval " << evaluate(pos_) << "\n";
}

bool Uci::handle_line(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
    if (cmd == "uci") cmd_uci();
    else if (cmd == "isready") std::cout << "readyok\n";
    else if (cmd == "ucinewgame") {
        pos_.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }
    else if (cmd == "position") cmd_position(rest);
    else if (cmd == "go") cmd_go(rest);
    else if (cmd == "perft") cmd_perft(rest);
    else if (cmd == "divide") cmd_divide(rest);
    else if (cmd == "bench") cmd_bench();
    else if (cmd == "d") cmd_display();
    else if (cmd == "eval") cmd_eval();
    else if (cmd == "setoption") {
        std::istringstream o(rest);
        std::string tok, name, value;
        o >> tok >> name >> tok >> value;
        try {
            if (name == "Threads" && !value.empty()) threads_ = std::stoi(value);
            if (name == "Hash" && !value.empty()) hash_mb_ = std::stoi(value);
        } catch (...) {}
    }
    else if (cmd == "quit" || cmd == "exit") return false;
    else if (cmd == "stop") searcher_.stop();
    return true;
}
