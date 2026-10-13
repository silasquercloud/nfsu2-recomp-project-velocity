#!/usr/bin/env python3
"""Summarize exported presentation intervals; no inferred GPU/runtime timings."""
import argparse
import csv
import math
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv")
    parser.add_argument("--budget-ms", type=float, default=1000 / 30)
    args = parser.parse_args()
    if not math.isfinite(args.budget_ms) or args.budget_ms <= 0:
        parser.error("budget must be a positive finite number")
    with open(args.csv, encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(line for line in source if not line.startswith("#")))
    if not rows:
        parser.error("no completed presentation intervals")
    wall = sorted(int(row["wall_ns"]) / 1e6 for row in rows)
    cpu = [int(row["cpu_process_ns"]) / 1e6 for row in rows]
    if wall[0] <= 0:
        parser.error("nonpositive interval; invalid report")
    def percentile(p):
        return wall[max(0, math.ceil(p * len(wall)) - 1)]
    print(f"Completed intervals: {len(rows)} (bounded recent history)")
    print(f"Frametime ms: p50={percentile(.5):.3f} p95={percentile(.95):.3f} "
          f"p99={percentile(.99):.3f} worst={wall[-1]:.3f}")
    print(f"Presentation rate in captured window: {1000/statistics.mean(wall):.2f}/s")
    print(f"Process CPU ms/interval: mean={statistics.mean(cpu):.3f} (all process threads)")
    print(f"Intervals over {args.budget_ms:.3f} ms: {sum(x > args.budget_ms for x in wall)}")
    timing_fields=[name for name in rows[0] if name.endswith("_ns") and name not in ("wall_ns","cpu_process_ns")]
    print("Host timing by path (overlapping/nested; not GPU execution):")
    for name in timing_fields:
        samples=sorted(int(row[name])/1e6 for row in rows)
        if samples[-1]>0:
            print(f"  {name}: mean={statistics.mean(samples):.3f} ms p95={samples[max(0,math.ceil(.95*len(samples))-1)]:.3f} ms max={samples[-1]:.3f} ms")
    for name in ("decoded_pixels","copied_bytes","texture_fallbacks","vulkan_errors","vulkan_suboptimal","swapchain_recreates"):
        if name in rows[0]:print(f"Captured-window {name}: {sum(int(row[name]) for row in rows)}")
    print("D3D9 timing is sampled; nested/parallel host timings overlap. "
          "No GPU timestamps or exact guest runtime attribution.")


if __name__ == "__main__":
    main()
