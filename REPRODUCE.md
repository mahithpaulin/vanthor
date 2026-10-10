# Reproduce (v0.0.1)

## Engine + tools verification

```bash
git clone https://github.com/mahithpaulin/vanthor && cd vanthor
git rev-parse HEAD                      # record this sha in any report
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2
sha256sum build/vanthor
python3 tests/test_uci.py --engine ./build/vanthor
python3 tools/elo.py --self-check
python3 tools/tournament.py --self-test --out tournaments/self-test
```

Expected: build succeeds, UCI smoke PASS, elo self-check PASS,
tournament self-test writes `tournaments/self-test/{manifest.json,games.pgn,summary.txt}`.

## Stockfish 19 reference binary (for future matches, not needed for v0.0.1 CI)

```bash
git clone https://github.com/official-stockfish/Stockfish && cd Stockfish
git checkout edb0d9db6731067ec50ce619ff372b463bc4dd5d   # sf_19
make -j ARCH=x86-64-avx2 && sha256sum stockfish
./stockfish uci   # check "id name Stockfish 19"
```

Record: commit sha, compiler version, flags, binary sha256, CPU model,
threads/hash, TC, openings, PGNs. See `docs/03-benchmark-protocol.md`.
