#!/usr/bin/env python3
"""UCI smoke test for the v0.0.1 Vanthor skeleton (stdlib only).

Spawns the engine, checks uci/isready/setoption/position/go/quit.
Exit 0 = pass, 1 = fail.
"""
import argparse
import re
import subprocess
import sys


def talk(proc, msg):
    proc.stdin.write(msg + "\n")
    proc.stdin.flush()


def wait_for(proc, pattern, timeout=10):
    out = []
    while True:
        line = proc.stdout.readline()
        if not line:
            return None, out
        line = line.rstrip("\n")
        out.append(line)
        if re.search(pattern, line):
            return line, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", required=True)
    args = ap.parse_args()
    proc = subprocess.Popen(
        [args.engine],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    failures = []

    def check(send, expect, label):
        talk(proc, send)
        got, _ = wait_for(proc, expect)
        if got is None:
            failures.append(label)
            print(f"FAIL {label}: no match for /{expect}/ after '{send}'")
        else:
            print(f"ok {label}: {got}")

    check("uci", r"^uciok$", "uci handshake")
    check("isready", r"^readyok$", "isready")
    talk(proc, "setoption name Threads value 2")
    talk(proc, "setoption name Hash value 64")
    check("isready", r"^readyok$", "options accepted")
    talk(proc, "position startpos moves e2e4 e7e5")
    talk(proc, "isready")
    got, _ = wait_for(proc, r"^readyok$")
    if got is None:
        failures.append("alive after position")
    else:
        print("ok alive after position")
    talk(proc, "go depth 1")
    got, _ = wait_for(proc, r"^bestmove [a-h][1-8][a-h][1-8][qrbn]?$")
    if got is None:
        failures.append("go returns bestmove")
        print("FAIL go returns bestmove")
    else:
        print(f"ok go returns bestmove: {got}")
    talk(proc, "quit")
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        failures.append("quit exits")
    print("RESULT:", "PASS" if not failures else f"FAIL {failures}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
