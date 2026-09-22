#!/usr/bin/env python3
# ===----------------------------------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===----------------------------------------------------------------------===#
"""Print markdown tables for a results directory written by run-bench.sh.

Usage: summarize-bench.py RESULTS_DIR

Reads RESULTS_DIR/json/{insert,traverse}-<backend>-...-r<run>.json. Where a
measurement was repeated, a cell is the median and the spread table gives
+/-(max - min) / (2 * median), which is what says whether two cells differ.
"""

import json
import os
import re
import statistics
import sys

ORDER = ["llvmcas", "lmdb", "sqlite", "file"]
NAME = re.compile(r"^(insert|traverse)-(\w+?)(?:-t(\d+))?-r(\d+)\.json$")


def gib(n):
    return f"{n / 2**30:.2f} GiB"


def seconds(s):
    return f"{s:.3f} s" if s < 10 else f"{s:.1f} s"


def spread(values):
    if len(values) < 2:
        return "—"
    return f"±{(max(values) - min(values)) / statistics.median(values) / 2:.1%}"


def main(results):
    inserts = {}  # backend -> [json]
    walks = {}  # (backend, threads) -> [seconds]
    visits = 0
    for f in sorted(os.listdir(os.path.join(results, "json"))):
        m = NAME.match(f)
        if not m:
            continue
        with open(os.path.join(results, "json", f)) as fh:
            j = json.load(fh)
        if m.group(1) == "insert":
            inserts.setdefault(m.group(2), []).append(j)
        else:
            walks.setdefault((m.group(2), int(m.group(3))), []).append(
                j["traverse"]["seconds"]
            )
            visits = j["traverse"]["nodes"]

    backends = [b for b in ORDER if b in inserts]
    threads = sorted({t for _, t in walks})
    out = []
    env = os.path.join(results, "environment.txt")
    if os.path.exists(env):
        out += ["```", open(env).read().rstrip(), "```", ""]

    data = next((js[0]["unique_data_bytes"] for js in inserts.values()), 0)
    out += [
        "### Size on disk",
        "",
        "| Backend | Allocated | vs object data | Apparent | Files | Dirs |",
        "|---|---|---|---|---|---|",
    ]
    for b in backends:
        s = inserts[b][0]["size"]
        out.append(
            f"| {b} | {gib(s['allocated_bytes'])} | "
            f"{s['allocated_bytes'] / data:.2f}x | {gib(s['apparent_bytes'])} | "
            f"{s['files']:,} | {s['dirs']:,} |"
        )

    out += [
        "",
        "### Insert, one thread",
        "",
        "| Backend | Runs | Median | Spread | Objects/s |",
        "|---|---|---|---|---|",
    ]
    for b in backends:
        secs = [j["insert"]["seconds"] for j in inserts[b]]
        med = statistics.median(secs)
        out.append(
            f"| {b} | {len(secs)} | {seconds(med)} | {spread(secs)} | "
            f"{inserts[b][0]['insert']['nodes'] / med:,.0f} |"
        )

    if walks:
        top = threads[-1]
        head = " | ".join(f"{t} thread{'s' if t > 1 else ''}" for t in threads)
        out += [
            "",
            "### Traversal",
            "",
            f"Visits per walk: {visits:,}.",
            "",
            f"| Backend | {head} | 1→{top} | M visits/s at {top} |",
            "|---|" + "---|" * (len(threads) + 2),
        ]
        for b in backends:
            med = [statistics.median(walks[(b, t)]) if (b, t) in walks else None
                   for t in threads]
            one, last = med[0], med[-1]
            out.append(
                f"| {b} | "
                + " | ".join(seconds(m) if m else "—" for m in med)
                + (f" | {one / last:.1f}x" if one and last else " | —")
                + (f" | {visits / last / 1e6:.3g} |" if last else " | — |")
            )
        out += [
            "",
            "Spread over the runs of each cell, ±(max - min) / (2 × median):",
            "",
            f"| Backend | {head} |",
            "|---|" + "---|" * len(threads),
        ]
        for b in backends:
            out.append(
                f"| {b} | "
                + " | ".join(spread(walks.get((b, t), [])) for t in threads)
                + " |"
            )
    out.append("")

    # A warning in a log can mean the number next to it is not usable, so
    # surface it rather than leave it for someone to find.
    warnings = []
    logs = os.path.join(results, "logs")
    for f in sorted(os.listdir(logs)):
        if not f.endswith(".log"):
            continue
        with open(os.path.join(logs, f), errors="replace") as fh:
            for line in sorted({l for l in fh if l.startswith("warning:")}):
                warnings.append(f"- `{f}`: {line.strip()}")
    if warnings:
        out += ["### Warnings", ""] + warnings + [""]

    print("\n".join(out))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
