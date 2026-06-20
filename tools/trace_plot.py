#!/usr/bin/env python3
# plots trace.csv: register values + pc movement over time.
# usage: python3 tools/trace_plot.py [trace.csv]
import sys
import csv
import matplotlib.pyplot as plt


def load(path):
    steps, pcs = [], []
    regs = {f"R{i}": [] for i in range(8)}
    with open(path) as f:
        for row in csv.DictReader(f):
            steps.append(int(row["step"]))
            pcs.append(int(row["pc"]))
            for i in range(8):
                regs[f"R{i}"].append(int(row[f"R{i}"]))
    return steps, pcs, regs


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "trace.csv"
    steps, pcs, regs = load(path)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 8), sharex=True)

    # --- registers over time -----------------------------------------
    for name, series in regs.items():
        if any(series):                      # skip registers that stay 0
            ax1.plot(steps, series, marker="o", markersize=3, label=name)
    ax1.set_title("Register values over execution")
    ax1.set_ylabel("value")
    ax1.legend(loc="upper left", ncol=4, fontsize=8)
    ax1.grid(True, alpha=0.3)

    # --- program counter ---------------------------------------------
    ax2.step(steps, pcs, where="post", color="crimson")
    ax2.set_title("Program counter (downward jumps = loop iterations)")
    ax2.set_xlabel("execution step")
    ax2.set_ylabel("PC (instruction #)")
    ax2.grid(True, alpha=0.3)

    fig.tight_layout()
    out = "trace_plot.png"
    fig.savefig(out, dpi=130)
    print(f"saved {out}")


if __name__ == "__main__":
    main()
