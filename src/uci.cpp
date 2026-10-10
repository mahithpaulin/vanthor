// VANTHOR uci.cpp — async protocol loop. Original implementation.
#include "uci.h"
#include "version.h"
#include "movegen.h"
#include "evaluate.h"
#include "perft.h"
#include <iostream>
#include <sstream>
#include <chrono>

Uci::Uci() = default;

Uci::~Uci() {
    search_.stop();
    if (worker_.joinable()) worker_.join();
    if (persist_hist_ && !hist_path_.empty()) search_.save_history(hist_path_);
}

void Uci::cmd_uci() {
    std::lock_guard<std::mutex> lk(out_m_);
    std::cout << "id name " << VANTHOR_NAME << " " << VANTHOR_VERSION << "\n";
    std::cout << "id author Project Vanthor\n";
    std::cout << "option name Hash type spin default 16 min 1 max 1024\n";
    std::cout << "option name Threads type spin default 1 min 1 max 8\n";
    std::cout << "option name NullMove type check default true\n";
    std::cout << "option name LMR type check default true\n";
    std::cout << "option name RFP type check default true\n";
    std::cout << "option name CheckExt type check default true\n";
    std::cout << "option name UseNNUE type check default true\n";
    std::cout << "option name EvalFile type string default <empty>\n";
    std::cout << "option name PersistHistory type check default false\n";
    std::cout << "option name HistoryFile type string default <empty>\n";
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
    if (searching_.load()) return;  // protocol violation; ignore while busy
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
        std::string::size_type p = rest.find(" moves ");
        std::string f = (p == std::string::npos) ? rest : rest.substr(0, p);
        if (p != std::string::npos) moves = rest.substr(p + 7);
        while (!f.empty() && f[0] == ' ') f.erase(0, 1);
        if (!pos_.set_fen(f)) {
            std::lock_guard<std::mutex> lk(out_m_);
            std::cout << "info string error: bad FEN\n";
            return;
        }
    } else return;
    if (!moves.empty() && !play_uci_moves(moves)) {
        std::lock_guard<std::mutex> lk(out_m_);
        std::cout << "info string error: illegal move in list\n";
    }
}

void Uci::print_info(const InfoLine& il) {
    std::lock_guard<std::mutex> lk(out_m_);
    std::cout << "info depth " << il.depth << " score cp " << il.score
              << " nodes " << il.nodes << " time " << il.ms << " nps "
              << (il.ms > 0 ? (il.nodes * 1000 / (uint64_t)il.ms) : il.nodes);
    if (!il.pv.empty()) {
        std::cout << " pv";
        for (Move m : il.pv) std::cout << " " << move_name(m);
    }
    std::cout << "\n";
}

void Uci::stop_search() {
    search_.stop();
    if (worker_.joinable()) worker_.join();
    searching_.store(false);
}

void Uci::cmd_go(const std::string& args) {
    if (searching_.load()) stop_search();  // stop any previous search first
    else if (worker_.joinable()) worker_.join();  // reap finished worker
    // (assigning a new thread to a joinable std::thread would terminate)
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
    if (movetime >= 0) {
        lim.soft_ms = movetime;
        lim.hard_ms = movetime;
    } else if ((pos_.side() == WHITE ? wtime : btime) >= 0 && !lim.infinite) {
        int64_t left = (pos_.side() == WHITE) ? wtime : btime;
        int64_t inc = (pos_.side() == WHITE) ? winc : binc;
        int64_t soft;
        if (movestogo > 0) soft = left / movestogo + inc / 2;
        else soft = left / 30 + inc / 2;
        if (soft < 10) soft = 10;
        int64_t hard = soft * 5;
        if (hard > left / 2 && left > 100) hard = left / 2;
        if (hard < soft) hard = soft;
        lim.soft_ms = soft;
        lim.hard_ms = hard;
    }
    if (!have_depth && lim.soft_ms < 0 && !lim.infinite && lim.max_nodes == UINT64_MAX)
        lim.depth = 8;  // bare `go`: keep it bounded

    Position root = pos_;
    int threads = threads_;
    SearchOptions opts = opts_;
    searching_.store(true);
    worker_ = std::thread([this, root, lim, threads, opts]() {
        uint64_t nodes = 0;
        Move best = search_.go(root, lim, threads, opts,
                               [this](const InfoLine& il) { print_info(il); },
                               &nodes);
        {
            std::lock_guard<std::mutex> lk(out_m_);
            std::cout << "bestmove " << move_name(best) << "\n";
            if (pending_ready_.exchange(false)) std::cout << "readyok\n";
        }
        searching_.store(false);
    });
}

void Uci::cmd_perft(const std::string& args) {
    if (searching_.load()) return;
    int depth = 0;
    try { depth = std::stoi(args); } catch (...) { return; }
    if (depth < 0 || depth > 8) {
        std::lock_guard<std::mutex> lk(out_m_);
        std::cout << "info string perft depth 0..8\n";
        return;
    }
    auto t0 = std::chrono::steady_clock::now();
    uint64_t nodes = perft(pos_, depth);
    auto t1 = std::chrono::steady_clock::now();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    uint64_t nps = ms > 0 ? (nodes * 1000 / (uint64_t)ms) : nodes;
    std::lock_guard<std::mutex> lk(out_m_);
    std::cout << "nodes " << nodes << " time " << ms << " nps " << nps << "\n";
}

void Uci::cmd_divide(const std::string& args) {
    if (searching_.load()) return;
    int depth = 0;
    try { depth = std::stoi(args); } catch (...) { return; }
    if (depth < 1 || depth > 7) {
        std::lock_guard<std::mutex> lk(out_m_);
        std::cout << "info string divide depth 1..7\n";
        return;
    }
    MoveList ml;
    generate_legal(pos_, ml);
    uint64_t total = 0;
    std::lock_guard<std::mutex> lk(out_m_);
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
    if (searching_.load()) return;
    const char* fens[2] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"};
    const int depths[2] = {10, 8};
    uint64_t total_nodes = 0;
    auto t0 = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(out_m_);
        for (int i = 0; i < 2; i++) {
            Position p;
            p.set_fen(fens[i]);
            SearchLimits lim;
            lim.depth = depths[i];
            lim.infinite = true;
            uint64_t nodes = 0;
            Search fresh;  // fresh TT: deterministic node counts
            Move best = fresh.go(p, lim, 1, opts_,
                                 [&](const InfoLine&) {}, &nodes);
            total_nodes += nodes;
            std::cout << "info string bench pos " << i << " depth " << depths[i]
                      << " nodes " << nodes << " bestmove " << move_name(best) << "\n";
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    uint64_t nps = ms > 0 ? (total_nodes * 1000 / (uint64_t)ms) : total_nodes;
    std::lock_guard<std::mutex> lk(out_m_);
    std::cout << "nodes " << total_nodes << " time " << ms << " nps " << nps << "\n";
}

void Uci::cmd_display() {
    if (searching_.load()) return;
    std::lock_guard<std::mutex> lk(out_m_);
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
    if (searching_.load()) return;
    std::lock_guard<std::mutex> lk(out_m_);
    std::cout << "eval hce " << evaluate(pos_);
    if (search_.net().loaded) {
        NnueState st;
        st.set_weights(&search_.net());
        st.full_refresh(pos_);
        std::cout << " nnue " << st.evaluate(pos_.side());
    }
    std::cout << "\n";
}

void Uci::cmd_save_history() {
    std::lock_guard<std::mutex> lk(out_m_);
    if (hist_path_.empty()) {
        std::cout << "info string SaveHistory: no HistoryFile set\n";
        return;
    }
    std::cout << "info string SaveHistory "
              << (search_.save_history(hist_path_) ? "ok " : "FAILED ")
              << hist_path_ << "\n";
}

void Uci::cmd_nnue_verify(const std::string& args) {
    // Incremental-accumulator integrity: random games, after every ply
    // compare incremental accs vs full recompute (tol 1e-3).
    std::lock_guard<std::mutex> lk(out_m_);
    if (hist_path_.empty()) {
        std::cout << "info string SaveHistory: no HistoryFile set\n";
        return;
    }
    std::cout << "info string SaveHistory "
              << (search_.save_history(hist_path_) ? "ok " : "FAILED ")
              << hist_path_ << "\n";
}
    if (searching_.load()) return;
    if (!search_.net().loaded) {
        std::lock_guard<std::mutex> lk(out_m_);
        std::cout << "nnue_verify FAIL no net loaded\n";
        return;
    }
    int games = 10;
    try { if (!args.empty()) games = std::stoi(args); } catch (...) {}
    if (games < 1) games = 1;
    if (games > 200) games = 200;
    uint64_t rng = 0x9E3779B97F4A7C15ULL;
    auto rnd = [&]() {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        return rng;
    };
    long plies = 0;
    std::lock_guard<std::mutex> lk(out_m_);
    for (int g = 0; g < games; g++) {
        Position p;
        NnueState st;
        st.set_weights(&search_.net());
        st.full_refresh(p);
        for (int i = 0; i < 300; i++) {
            MoveList ml;
            generate_legal(p, ml);
            if (ml.n == 0) break;
            Move m = ml.m[rnd() % (uint64_t)ml.n];
            // mirror Worker's capture bookkeeping
            int cap_pt = NO_PIECE, cap_sq = -1;
            if (move_is_capture(m)) {
                int to = move_to(m);
                if (move_is_ep(m)) {
                    cap_pt = PAWN;
                    cap_sq = to + (p.side() == WHITE ? -8 : 8);
                } else {
                    int v = p.piece_on(to);
                    if (v >= 0) {
                        cap_pt = v % 6;
                        cap_sq = to;
                    }
                }
            }
            Color us = p.side();
            p.do_move(m);
            st.apply(p, m, us, cap_pt, cap_sq);
            plies++;
            if (!st.verify_acc(p, 1e-3f)) {
                std::cout << "nnue_verify FAIL acc mismatch game " << g
                          << " ply " << i << " " << move_name(m) << "\n";
                return;
            }
        }
    }
    std::cout << "nnue_verify ok " << plies << "\n";
}

void Uci::cmd_keytest(const std::string& args) {
    if (searching_.load()) return;
    int games = 20;
    try { if (!args.empty()) games = std::stoi(args); } catch (...) {}
    if (games < 1) games = 1;
    if (games > 500) games = 500;
    uint64_t rng = 0x243F6A8885A308D3ULL;
    auto rnd = [&]() {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        return rng;
    };
    long plies = 0;
    std::lock_guard<std::mutex> lk(out_m_);
    for (int g = 0; g < games; g++) {
        Position p;
        for (int i = 0; i < 300; i++) {
            MoveList ml;
            generate_legal(p, ml);
            if (ml.n == 0) break;
            Move m = ml.m[rnd() % (uint64_t)ml.n];
            p.do_move(m);
            plies++;
            if (!p.key_ok()) {
                std::cout << "keytest FAIL incremental key mismatch game " << g
                          << " ply " << i << " " << move_name(m) << "\n";
                return;
            }
            Position q;
            if (!q.set_fen(p.fen()) || q.key() != p.key() ||
                q.fen() != p.fen()) {
                std::cout << "keytest FAIL fen round-trip game " << g
                          << " ply " << i << "\n";
                return;
            }
        }
    }
    std::cout << "keytest ok " << plies << "\n";
}

bool Uci::handle_line(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
    if (cmd == "uci") cmd_uci();
    else if (cmd == "isready") {
        if (searching_.load()) pending_ready_.store(true);
        else {
            std::lock_guard<std::mutex> lk(out_m_);
            std::cout << "readyok\n";
        }
    }
    else if (cmd == "ucinewgame") {
        stop_search();
        search_.tt().clear();
        pos_.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }
    else if (cmd == "position") cmd_position(rest);
    else if (cmd == "go") cmd_go(rest);
    else if (cmd == "stop") {
        if (searching_.load()) stop_search();
    }
    else if (cmd == "perft") cmd_perft(rest);
    else if (cmd == "divide") cmd_divide(rest);
    else if (cmd == "bench") cmd_bench();
    else if (cmd == "d") cmd_display();
    else if (cmd == "eval") cmd_eval();
    else if (cmd == "keytest") cmd_keytest(rest);
    else if (cmd == "nnue_verify") cmd_nnue_verify(rest);
    else if (cmd == "savehistory") cmd_save_history();
    else if (cmd == "setoption") {
        std::istringstream o(rest);
        std::string tok, name, value, id;
        o >> tok;  // "name"
        // name is one token here (Hash/Threads/NullMove/LMR/RFP/CheckExt/...)
        o >> name;
        if (name == "EvalFile" || name == "HistoryFile") {
            // file path: take the remainder of the line after "value"
            std::string::size_type p = rest.find("value");
            value = (p == std::string::npos) ? "" : rest.substr(p + 5);
            while (!value.empty() && value[0] == ' ') value.erase(0, 1);
            while (!value.empty() && value.back() == ' ') value.pop_back();
            if (!searching_.load()) {
                if (name == "EvalFile") {
                    if (value.empty() || value == "<empty>" || value == "<none>") {
                        search_.net().loaded = false;
                    } else {
                        bool ok = search_.net().load(value);
                        std::lock_guard<std::mutex> lk(out_m_);
                        std::cout << "info string EvalFile "
                                  << (ok ? "loaded " : "FAILED ") << value << "\n";
                    }
                } else {  // HistoryFile
                    hist_path_ = (value == "<empty>" || value == "<none>") ? "" : value;
                    if (!hist_path_.empty() && persist_hist_) {
                        bool ok = search_.load_history(hist_path_);
                        std::lock_guard<std::mutex> lk(out_m_);
                        std::cout << "info string HistoryFile "
                                  << (ok ? "loaded " : "new ") << hist_path_ << "\n";
                    }
                }
            }
        } else {
            o >> tok;  // "value"
            o >> value;
            try {
                if (name == "Threads" && !value.empty()) {
                    int t = std::stoi(value);
                    if (t >= 1 && t <= 8 && !searching_.load()) threads_ = t;
                } else if (name == "Hash" && !value.empty()) {
                    int h = std::stoi(value);
                    if (!searching_.load()) search_.tt().resize(h);
                } else if (name == "NullMove") opts_.nullmove = (value == "true");
                else if (name == "LMR") opts_.lmr = (value == "true");
                else if (name == "RFP") opts_.rfp = (value == "true");
                else if (name == "CheckExt") opts_.checkext = (value == "true");
                else if (name == "UseNNUE") search_.set_use_nnue(value == "true");
                else if (name == "PersistHistory") {
                    persist_hist_ = (value == "true");
                    if (persist_hist_ && !hist_path_.empty() && !searching_.load()) {
                        bool ok = search_.load_history(hist_path_);
                        std::lock_guard<std::mutex> lk(out_m_);
                        std::cout << "info string HistoryFile "
                                  << (ok ? "loaded " : "new ") << hist_path_ << "\n";
                    }
                }
            } catch (...) {}
        }
        (void)id;
    }
    else if (cmd == "quit" || cmd == "exit") {
        stop_search();
        return false;
    }
    return true;
}
