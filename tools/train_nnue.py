#!/usr/bin/env python3
"""HalfKP NNUE trainer, single-file (Kaggle script-kernel compatible: torch
+ numpy + stdlib only). Data format from tools/gen_data.py:
FEN | score_cp | result

Features (HalfKP, oriented): white list = white-king perspective, black
list = black-king perspective with king index AND squares flipped so that
black_idx(pos) == white_idx(color_flip(pos)).

Model: acc_w = sum ft[w], acc_b = sum ft[b]; order halves by side to move
(first = stm perspective); h = clip(concat, 0, 1); out = Linear(2H, 1).
Loss: MSE on teacher cp (pawns, clamped) + 0.1 * MSE(sigmoid(pred/0.6), WDL).

Export (little-endian): magic "VNTH1", u32 H, float32 ft [41024,H],
float32 ft_bias [H] (zeros in v1), float32 out_w [H] (stm-half weights),
float32 out_b [H], float32 out_bias [1].
"""
import argparse
import os
import struct
import sys
import time

FEN_PIECES = {"p": 0, "n": 1, "b": 2, "r": 3, "q": 4}


def halfkp_indices(fen):
    parts = fen.split()
    board = parts[0]
    wk = bk = -1
    pieces = []
    rank, file = 7, 0
    for ch in board:
        if ch == "/":
            rank -= 1
            file = 0
        elif ch.isdigit():
            file += int(ch)
        else:
            color = 0 if ch.isupper() else 1
            sq = rank * 8 + file
            low = ch.lower()
            if low == "k":
                if color == 0:
                    wk = sq
                else:
                    bk = sq
            else:
                pieces.append((color, FEN_PIECES[low], sq))
            file += 1
    w, b = [], []
    for (color, pt, sq) in pieces:
        slot_w = (0 if color == 0 else 1) * 5 + pt
        w.append(wk * 641 + slot_w * 64 + sq)
        slot_b = (0 if color == 1 else 1) * 5 + pt
        b.append((bk ^ 56) * 641 + slot_b * 64 + (sq ^ 56))
    stm = 0 if parts[1] == "w" else 1
    return w, b, stm


def load_data(path, max_rows=0):
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                fen, cp, res = [x.strip() for x in line.split("|")]
                cp = max(-1500.0, min(1500.0, float(cp)))
                res = float(res)
            except ValueError:
                continue
            rows.append((fen, cp / 1000.0, res))
            if max_rows and len(rows) >= max_rows:
                break
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True)
    ap.add_argument("--hidden", type=int, default=128)
    ap.add_argument("--epochs", type=int, default=50)
    ap.add_argument("--batch", type=int, default=4096)
    ap.add_argument("--lr", type=float, default=0.001)
    ap.add_argument("--val-frac", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--max-rows", type=int, default=0)
    ap.add_argument("--out", required=True)
    ap.add_argument("--device", default="")
    args = ap.parse_args()

    import numpy as np
    import torch
    import torch.nn as nn

    torch.manual_seed(args.seed)
    dev = args.device or ("cuda" if torch.cuda.is_available() else "cpu")
    device = torch.device(dev)
    print(f"device={device} cuda={torch.cuda.is_available()}", flush=True)

    rows = load_data(args.data, args.max_rows)
    print(f"rows={len(rows)}", flush=True)
    if len(rows) < 100:
        print("too few rows")
        return 2

    H = args.hidden

    class Net(nn.Module):
        def __init__(self):
            super().__init__()
            self.ft = nn.EmbeddingBag(41024, H, mode="sum")
            self.out = nn.Linear(2 * H, 1)
            nn.init.uniform_(self.ft.weight, -0.02, 0.02)
            nn.init.uniform_(self.out.weight, -0.02, 0.02)
            nn.init.zeros_(self.out.bias)

        def bags(self, lists):
            flat = torch.tensor([i for sub in lists for i in sub],
                                dtype=torch.long)
            offs = torch.zeros(len(lists), dtype=torch.long)
            s = 0
            for j, sub in enumerate(lists):
                offs[j] = s
                s += len(sub)
            return flat.to(device), offs.to(device)

        def forward(self, w_lists, b_lists, stm):
            wf, wo = self.bags(w_lists)
            bf, bo = self.bags(b_lists)
            acc_w = self.ft(wf, wo)
            acc_b = self.ft(bf, bo)
            first = torch.where((stm == 0).view(-1, 1), acc_w, acc_b)
            second = torch.where((stm == 0).view(-1, 1), acc_b, acc_w)
            h = torch.clamp(torch.cat([first, second], dim=1), 0.0, 1.0)
            return self.out(h).squeeze(-1)

    # precompute index lists once (CPU ram; fine to ~2M rows)
    W = [None] * len(rows)
    B = [None] * len(rows)
    CP = np.zeros(len(rows), dtype=np.float32)
    WDL = np.zeros(len(rows), dtype=np.float32)
    STM = np.zeros(len(rows), dtype=np.int64)
    for i, (fen, cp, res) in enumerate(rows):
        w, b, stm = halfkp_indices(fen)
        W[i], B[i], STM[i] = w, b, stm
        CP[i], WDL[i] = cp, res
    CPt = torch.tensor(CP).to(device)
    WDLt = torch.tensor(WDL).to(device)
    STMt = torch.tensor(STM).to(device)

    n_val = max(10, int(len(rows) * args.val_frac))
    tri = list(range(n_val, len(rows)))
    vai = list(range(n_val))

    net = Net().to(device)
    opt = torch.optim.Adam(net.parameters(), lr=args.lr)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, args.epochs)
    mse = nn.MSELoss()
    Bsz = args.batch
    best_val = float("inf")
    for ep in range(args.epochs):
        net.train()
        perm = torch.randperm(len(tri)).tolist()
        tot, cnt = 0.0, 0
        t0 = time.time()
        for i in range(0, len(tri), Bsz):
            bi = [tri[j] for j in perm[i:i + Bsz]]
            wb = [W[j] for j in bi]
            bb = [B[j] for j in bi]
            pred = net.forward(wb, bb, STMt[bi])
            tgt = CPt[bi]
            wdl = WDLt[bi]
            loss = mse(pred, tgt) + 0.1 * mse(torch.sigmoid(pred / 0.6), wdl)
            opt.zero_grad()
            loss.backward()
            opt.step()
            tot += loss.item() * len(bi)
            cnt += len(bi)
        sched.step()
        net.eval()
        with torch.no_grad():
            vp = torch.cat([
                net.forward([W[j] for j in vai[k:k + Bsz]],
                            [B[j] for j in vai[k:k + Bsz]],
                            STMt[vai[k:k + Bsz]])
                for k in range(0, len(vai), Bsz)])
            vloss = mse(vp, CPt[vai]).item()
        el = time.time() - t0
        print(f"epoch {ep+1}/{args.epochs} train={tot/cnt:.5f} "
              f"val_mse={vloss:.5f} ({el:.0f}s)", flush=True)
        if vloss < best_val:
            best_val = vloss
            torch.save(net.state_dict(), args.out + ".pt")
    print(f"best_val_mse={best_val:.5f}", flush=True)

    sd = torch.load(args.out + ".pt", map_location="cpu")
    ft = sd["ft.weight"].numpy()
    ow = sd["out.weight"].numpy().reshape(2 * H)
    ob = sd["out.bias"].numpy()
    with open(args.out, "wb") as f:
        f.write(b"VNTH1")
        f.write(struct.pack("<I", H))
        f.write(ft.astype("<f4").tobytes())
        f.write(np.zeros(H).astype("<f4").tobytes())
        f.write(ow[:H].astype("<f4").tobytes())
        f.write(ow[H:].astype("<f4").tobytes())
        f.write(ob.astype("<f4").tobytes())
    print(f"exported {args.out} ({os.path.getsize(args.out)} bytes)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
