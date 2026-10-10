#!/usr/bin/env python3
"""Cross-check: python forward pass of a VNTH1 net vs the C++ engine eval.

Usage: check_nnue.py --net net.bin --fen "FEN"
Prints python cp. Compare with engine: setoption EvalFile + eval.
Agreement within a few cp = feature mapping matches. Large gap = bug.
"""
import argparse
import struct
import sys

import numpy as np

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
from train_nnue import halfkp_indices  # noqa: E402


def load_net(path):
    with open(path, "rb") as f:
        assert f.read(5) == b"VNTH1"
        (h,) = struct.unpack("<I", f.read(4))
        ft = np.frombuffer(f.read(41024 * h * 4), dtype="<f4").reshape(41024, h)
        fbb = np.frombuffer(f.read(h * 4), dtype="<f4")
        ow = np.frombuffer(f.read(h * 4), dtype="<f4")
        ob = np.frombuffer(f.read(h * 4), dtype="<f4")
        bias = np.frombuffer(f.read(4), dtype="<f4")[0]
    return h, ft, fbb, ow, ob, bias


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--net", required=True)
    ap.add_argument("--fen", required=True)
    args = ap.parse_args()
    h, ft, fbb, ow, ob, bias = load_net(args.net)
    w, b, stm = halfkp_indices(args.fen)
    acc_w = fbb + ft[w].sum(axis=0)
    acc_b = fbb + ft[b].sum(axis=0)
    first, second = (acc_w, acc_b) if stm == 0 else (acc_b, acc_w)
    s = bias + np.clip(first, 0, 1).dot(ow) + np.clip(second, 0, 1).dot(ob)
    print(f"python nnue cp = {s * 1000:.1f} (stm={'w' if stm == 0 else 'b'})")


if __name__ == "__main__":
    sys.exit(main())
