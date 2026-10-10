#include "uci.h"
#include "version.h"
#include <iostream>
#include <sstream>

Uci::Uci() = default;

void Uci::cmd_uci() {
    std::cout << "id name " << VANTHOR_NAME << " " << VANTHOR_VERSION
              << " (" << VANTHOR_STATUS << ")\n";
    std::cout << "id author Project Vanthor\n";
    std::cout << "option name Hash type spin default 16 min 1 max 1024\n";
    std::cout << "option name Threads type spin default 1 min 1 max 8\n";
    std::cout << "uciok\n";
}

void Uci::cmd_isready() { std::cout << "readyok\n"; }

void Uci::cmd_position(const std::string& args) {
    // v0.0.1: record only. Full FEN parsing lands with position.* in v0.1.0.
    fen_ = args.empty() ? "startpos" : args;
}

void Uci::cmd_go(const std::string& /*args*/) {
    // STUB: deterministic placeholder. Real search arrives in v0.2.0.
    // e2e4 is always legal from startpos; from other positions this stub
    // may emit an illegal move — callers must treat v0.0.1 as non-playing.
    std::cerr << "info string vanthor-0.0.1 stub: no search yet\n";
    std::cout << "bestmove e2e4\n";
}

bool Uci::handle_line(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
    if (cmd == "uci") cmd_uci();
    else if (cmd == "isready") cmd_isready();
    else if (cmd == "ucinewgame") { /* no state yet */ }
    else if (cmd == "position") cmd_position(rest);
    else if (cmd == "go") cmd_go(rest);
    else if (cmd == "setoption") {
        // setoption name <id> value <x>
        std::istringstream o(rest);
        std::string tok, name, value;
        o >> tok; // "name"
        o >> name;
        o >> tok; // "value"
        o >> value;
        try {
            if (name == "Threads" && !value.empty()) threads_ = std::stoi(value);
            if (name == "Hash" && !value.empty()) hash_mb_ = std::stoi(value);
        } catch (...) { /* keep defaults on bad input */ }
    } else if (cmd == "quit" || cmd == "exit") return false;
    // unknown commands (incl. bench/perft/debug) ignored in stub
    return true;
}
