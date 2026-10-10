#pragma once
#include <string>

// Minimal UCI front-end. v0.0.1: protocol plumbing only; search arrives v0.2.0.
// handle_line returns false when the loop should exit.
class Uci {
public:
    Uci();
    bool handle_line(const std::string& line);
    const std::string& fen() const { return fen_; }
    int threads() const { return threads_; }
    int hash_mb() const { return hash_mb_; }
private:
    void cmd_uci();
    void cmd_isready();
    void cmd_position(const std::string& args);
    void cmd_go(const std::string& args);
    std::string fen_ = "startpos";
    int threads_ = 1;
    int hash_mb_ = 16;
};
