#!/usr/bin/env python3
"""
run_ns3_campaign.py

Tier-A ns-3.40 Physical Scalability Campaign for ConCGR-AoI (Research #2)
Executes:
  1. Swarm Scalability Sweep: N in {10, 20, 30, 40} UAVs x 6 Protocols x 3 Seeds
  2. Buffer Capacity Sweep: B in {2, 4, 8, 16} MB x 5 Protocols x 3 Seeds
  Output: /home/jarvis/concgr_dtn_ws/data/ns3_scalability_results.csv
"""

import subprocess
import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed

NS3_EXECUTABLE = "/home/jarvis/ns-3-dev/build/scratch/ns3.40-concgr-dtn-uav-sim-optimized"
OUTPUT_CSV = "/home/jarvis/concgr_dtn_ws/data/ns3_scalability_results.csv"

PROTOCOLS = [
    "ConCGR-AoI",
    "Deterministic-CGR",
    "PRoPHETv2",
    "Spray-and-Wait",
    "Epidemic",
    "Connected-FANET"
]

SCALABILITY_UAV_COUNTS = [10, 20, 30, 40]
SEEDS = [42, 101, 777]

BUFFER_SIZES_MB = [2.0, 4.0, 8.0, 16.0]
BUFFER_SWEEP_PROTOCOLS = ["ConCGR-AoI", "Deterministic-CGR", "PRoPHETv2", "Spray-and-Wait", "Epidemic"]

def run_simulation(task):
    proto, num_uavs, buffer_mb, seed = task
    temp_csv = f"/tmp/ns3_{proto}_{num_uavs}_{int(buffer_mb)}_{seed}.csv"
    if os.path.exists(temp_csv):
        os.remove(temp_csv)

    cmd = [
        NS3_EXECUTABLE,
        f"--protocol={proto}",
        f"--numUavs={num_uavs}",
        f"--bufferCapMB={buffer_mb}",
        f"--seed={seed}",
        "--simTime=1200",
        f"--outputFile={temp_csv}"
    ]
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True)
    t1 = time.time()
    status = "SUCCESS" if res.returncode == 0 else f"FAILED({res.returncode})"

    csv_line = None
    if os.path.exists(temp_csv):
        with open(temp_csv, "r") as f:
            lines = [l.strip() for l in f if l.strip()]
            if len(lines) >= 2:
                csv_line = lines[-1]
        os.remove(temp_csv)

    return (proto, num_uavs, buffer_mb, seed, status, t1 - t0, csv_line)

def main():
    print("=" * 80)
    print("ns-3.40 Co-Simulation Scalability & Buffer Stress Campaign")
    print("=" * 80, flush=True)

    tasks = []
    # 1. Swarm Scalability Sweep (Buffer = 8 MB)
    for n in SCALABILITY_UAV_COUNTS:
        for proto in PROTOCOLS:
            for s in SEEDS:
                tasks.append((proto, n, 8.0, s))

    # 2. Buffer Capacity Sweep (N = 20 UAVs)
    for b in BUFFER_SIZES_MB:
        if b == 8.0:
            continue
        for proto in BUFFER_SWEEP_PROTOCOLS:
            for s in SEEDS:
                tasks.append((proto, 20, b, s))

    print(f"Total Simulation Missions to Execute: {len(tasks)}")
    print(f"Parallel Worker Threads: 6", flush=True)
    print("-" * 80, flush=True)

    header = "Protocol,NumUavs,BufferCapMB,Seed,TotalGen,Delivered,BDR_Pct,DMR_Pct,P50_AoI_s,P95_AoI_s,P99_AoI_s,Overhead,TotalPackets,MacCollisions,MacDrops,CollisionProb_Pct,AvgSolveTime_us\n"
    os.makedirs(os.path.dirname(OUTPUT_CSV), exist_ok=True)
    with open(OUTPUT_CSV, "w") as f:
        f.write(header)

    start_time = time.time()
    completed = 0
    valid_count = 0

    with ProcessPoolExecutor(max_workers=6) as executor:
        futures = {executor.submit(run_simulation, task): task for task in tasks}
        for future in as_completed(futures):
            proto, n, b, s, status, duration, line = future.result()
            completed += 1
            if line:
                valid_count += 1
                with open(OUTPUT_CSV, "a") as f:
                    f.write(line + "\n")
            print(f"[{completed:3d}/{len(tasks):3d}] {proto:<18} | N={n:2d} | B={b:4.1f}MB | Seed={s:3d} -> {status} ({duration:.2f}s)", flush=True)

    total_time = time.time() - start_time
    print("=" * 80, flush=True)
    print(f"Campaign Complete! Total wall-clock time: {total_time:.2f} s ({total_time/60.0:.2f} min)", flush=True)
    print(f"Valid results collected: {valid_count} / {len(tasks)}", flush=True)
    print(f"Unified CSV saved to: {OUTPUT_CSV}", flush=True)
    print("=" * 80, flush=True)

if __name__ == "__main__":
    main()
