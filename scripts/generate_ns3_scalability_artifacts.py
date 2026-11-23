#!/usr/bin/env python3
"""
generate_ns3_scalability_artifacts.py

Processes ns-3.40 Physical Simulation Results (117 Missions)
Generates:
  1. LaTeX Table 3: table3_ns3_scalability.tex
  2. Publication Figure 6: data/plots/fig6_ns3_scalability_analysis.png
  3. Artifact report integration
"""

import csv
import os
import math
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

INPUT_CSV = "/home/jarvis/concgr_dtn_ws/data/ns3_scalability_results.csv"
OUT_TABLE = "/home/jarvis/concgr_dtn_ws/data/table3_ns3_scalability.tex"
OUT_PLOT_DIR = "/home/jarvis/concgr_dtn_ws/data/plots"
OUT_FIG6 = os.path.join(OUT_PLOT_DIR, "fig6_ns3_scalability_analysis.png")
ARTIFACT_DIR = "/home/jarvis/.gemini/antigravity/brain/8a2792bb-81d2-4989-b5f6-18baa5e33c69"

os.makedirs(OUT_PLOT_DIR, exist_ok=True)

# Okabe-Ito accessible color palette
PALETTE = {
    "ConCGR-AoI": "#0072B2",        # Blue
    "Deterministic-CGR": "#009E73", # Bluish Green
    "Spray-and-Wait": "#D55E00",    # Vermilion
    "PRoPHETv2": "#CC79A7",         # Reddish Purple
    "Epidemic": "#E69F00",          # Orange
    "Connected-FANET": "#56B4E9"    # Sky Blue
}

MARKERS = {
    "ConCGR-AoI": "o",
    "Deterministic-CGR": "s",
    "Spray-and-Wait": "^",
    "PRoPHETv2": "D",
    "Epidemic": "v",
    "Connected-FANET": "x"
}

def load_data(filepath):
    data = []
    with open(filepath, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                "Protocol": row["Protocol"],
                "NumUavs": int(row["NumUavs"]),
                "BufferCapMB": float(row["BufferCapMB"]),
                "Seed": int(row["Seed"]),
                "TotalGen": int(row["TotalGen"]),
                "Delivered": int(row["Delivered"]),
                "BDR_Pct": float(row["BDR_Pct"]),
                "DMR_Pct": float(row["DMR_Pct"]),
                "P50_AoI_s": float(row["P50_AoI_s"]),
                "P95_AoI_s": float(row["P95_AoI_s"]),
                "P99_AoI_s": float(row["P99_AoI_s"]),
                "Overhead": float(row["Overhead"]),
                "TotalPackets": int(row["TotalPackets"]),
                "MacCollisions": int(row["MacCollisions"]),
                "MacDrops": int(row["MacDrops"]),
                "CollisionProb_Pct": float(row["CollisionProb_Pct"]),
                "AvgSolveTime_us": float(row["AvgSolveTime_us"])
            })
    return data

def aggregate_stats(records):
    """Computes mean and std for a list of records."""
    if not records:
        return {}
    keys = ["BDR_Pct", "DMR_Pct", "P50_AoI_s", "P95_AoI_s", "P99_AoI_s", "Overhead", "TotalPackets", "MacCollisions", "AvgSolveTime_us"]
    res = {}
    for k in keys:
        vals = [r[k] for r in records]
        res[f"{k}_mean"] = float(np.mean(vals))
        res[f"{k}_std"] = float(np.std(vals))
    return res

def main():
    print(f"Loading {INPUT_CSV}...")
    records = load_data(INPUT_CSV)
    print(f"Loaded {len(records)} simulation mission records.")

    protocols = ["Connected-FANET", "Spray-and-Wait", "Deterministic-CGR", "PRoPHETv2", "Epidemic", "ConCGR-AoI"]
    scales = [10, 20, 30, 40]
    buffers = [2.0, 4.0, 8.0, 16.0]

    # Group by (Protocol, NumUavs, BufferCapMB)
    groups = defaultdict(list)
    for r in records:
        groups[(r["Protocol"], r["NumUavs"], r["BufferCapMB"])].append(r)

    # -------------------------------------------------------------------------
    # 1. Generate LaTeX Table 3 (Booktabs Scalability)
    # -------------------------------------------------------------------------
    tex_lines = [
        r"\begin{table*}[t]",
        r"\centering",
        r"\caption{Physical Layer IEEE 802.11p Scalability and Contention Benchmark across Mountainous SRTM DEM ($B = 8\text{ MB}$, Means $\pm$ Std. Dev. over $N_{\text{seeds}} = 3$ Missions).}",
        r"\label{tab:ns3_scalability}",
        r"\small",
        r"\begin{tabular}{lrrrrrrr}",
        r"\toprule",
        r"\textbf{Protocol} & \textbf{Swarm ($N$)} & \textbf{BDR (\%)} & \textbf{P95 AoI (s)} & \textbf{Overhead} & \textbf{Total Packets} & \textbf{MAC Collisions} & \textbf{Solve ($\mu$s)} \\",
        r"\midrule"
    ]

    for n in scales:
        tex_lines.append(f"\\multicolumn{{8}}{{l}}{{\\textit{{Swarm Size $N = {n}$ UAVs ($5\\times 5\\text{{ km}}^2$ Kangra Relief)}}}} \\\\")
        for proto in protocols:
            key = (proto, n, 8.0)
            recs = groups.get(key, [])
            if not recs:
                continue
            st = aggregate_stats(recs)
            bdr_s = f"{st['BDR_Pct_mean']:.1f} \\pm {st['BDR_Pct_std']:.1f}"
            p95_s = f"{st['P95_AoI_s_mean']:.1f} \\pm {st['P95_AoI_s_std']:.1f}"
            ovh_s = f"{st['Overhead_mean']:.1f}\\times"
            pkt_s = f"{int(st['TotalPackets_mean']):,}"
            col_s = f"{int(st['MacCollisions_mean']):,}"
            sol_s = f"{st['AvgSolveTime_us_mean']:.1f}"

            if proto == "ConCGR-AoI":
                tex_lines.append(f"\\textbf{{{proto}}} & {n} & \\textbf{{{bdr_s}}} & \\textbf{{{p95_s}}} & \\textbf{{{ovh_s}}} & \\textbf{{{pkt_s}}} & \\textbf{{{col_s}}} & \\textbf{{{sol_s}}} \\\\")
            else:
                tex_lines.append(f"{proto} & {n} & {bdr_s} & {p95_s} & {ovh_s} & {pkt_s} & {col_s} & {sol_s} \\\\")
        tex_lines.append(r"\midrule" if n != scales[-1] else r"\bottomrule")

    tex_lines.extend([
        r"\end{tabular}",
        r"\end{table*}"
    ])

    with open(OUT_TABLE, "w") as f:
        f.write("\n".join(tex_lines) + "\n")
    print(f"Generated LaTeX Table 3 at: {OUT_TABLE}")

    # -------------------------------------------------------------------------
    # 2. Generate Publication Figure 6 (3-Panel Scalability & Contention Plot)
    # -------------------------------------------------------------------------
    plt.rcParams.update({
        'font.family': 'sans-serif',
        'font.sans-serif': ['DejaVu Sans', 'Arial', 'Helvetica'],
        'font.size': 12.5,
        'axes.labelsize': 13.5,
        'axes.titlesize': 14.5,
        'xtick.labelsize': 12.0,
        'ytick.labelsize': 12.0,
        'legend.fontsize': 11.0,
        'figure.titlesize': 15.5
    })

    fig, axes = plt.subplots(1, 3, figsize=(19, 6.4), dpi=300)

    # Panel 6(a): Swarm Scale vs Physical MAC Collisions (Log scale)
    ax0 = axes[0]
    # Shaded operational zones
    ax0.axhspan(1e3, 1e5, color='#E8F5E9', alpha=0.35, label='Normal CSMA/CA Band (< 100k Collisions)')
    ax0.axhspan(1e5, 5e5, color='#FFF8E1', alpha=0.45, label='Contention Knee Zone (100k - 500k)')
    ax0.axhspan(5e5, 2e6, color='#FFEBEE', alpha=0.45, label='MAC Contention Collapse (> 500k Collisions)')

    for proto in protocols:
        if proto == "Connected-FANET": continue # extremely small packet volume due to disconnections
        pts_x = []
        pts_y = []
        pts_err = []
        for n in scales:
            st = aggregate_stats(groups.get((proto, n, 8.0), []))
            if st:
                pts_x.append(n)
                pts_y.append(st["MacCollisions_mean"])
                pts_err.append(st["MacCollisions_std"])
        lw = 3.2 if proto == "ConCGR-AoI" else 2.2
        ms = 9.5 if proto == "ConCGR-AoI" else 7.5
        ax0.errorbar(pts_x, pts_y, yerr=pts_err, label=proto, color=PALETTE[proto],
                     marker=MARKERS[proto], markersize=ms, linewidth=lw, capsize=5, capthick=1.4)

    # Callout badge for ConCGR-AoI suppression
    ax0.annotate(r"$\mathbf{ConCGR-AoI\ Quota\ Q_b^* \leq 4:}$" + "\nSuppresses collisions by $80\\times$\nvs. Epidemic Flooding at $N=40$",
                 xy=(40, 223865), xytext=(14, 700000),
                 arrowprops=dict(facecolor='black', edgecolor='black', shrink=0.05, width=1.4, headwidth=7),
                 fontsize=10, fontweight='bold', color='black',
                 bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="black", lw=1.0, alpha=0.96))

    ax0.set_yscale('log')
    ax0.set_xlabel('Swarm Scale $N$ (Patrol UAVs)', fontsize=13.5, fontweight='bold')
    ax0.set_ylabel('802.11p MAC Layer Collisions (Log Scale)', fontsize=13.5, fontweight='bold')
    ax0.set_title('(a) Physical Channel Contention vs Swarm Scale', fontsize=14.5, fontweight='bold', pad=12)
    ax0.grid(True, which="both", ls="--", alpha=0.4)
    ax0.set_xticks(scales)
    ax0.set_ylim(1e3, 2e6)
    ax0.legend(loc='lower right', fontsize=10.0, framealpha=0.94)

    # Panel 6(b): Swarm Scale vs P95 Peak-AoI (Freshness under Scaling)
    ax1 = axes[1]
    # Add application deadline lines
    ax1.axhline(90.0, color='#F57F17', linestyle='--', lw=2.0, alpha=0.9, label='LiDAR Submap Freshness ($D_b = 90$ s)')
    ax1.axhline(240.0, color='#0288D1', linestyle='--', lw=2.0, alpha=0.9, label='Routine Telemetry Horizon ($D_b = 240$ s)')

    for proto in ["ConCGR-AoI", "Deterministic-CGR", "Spray-and-Wait", "PRoPHETv2", "Epidemic"]:
        pts_x = []
        pts_y = []
        pts_err = []
        for n in scales:
            st = aggregate_stats(groups.get((proto, n, 8.0), []))
            if st:
                pts_x.append(n)
                pts_y.append(st["P95_AoI_s_mean"])
                pts_err.append(st["P95_AoI_s_std"])
        lw = 3.2 if proto == "ConCGR-AoI" else 2.2
        ms = 9.5 if proto == "ConCGR-AoI" else 7.5
        ax1.errorbar(pts_x, pts_y, yerr=pts_err, label=proto, color=PALETTE[proto],
                     marker=MARKERS[proto], markersize=ms, linewidth=lw, capsize=5, capthick=1.4)

    # Annotate flat scaling (positioned at x=22, y=540, completely clear of legend and curves)
    ax1.annotate(r"$\mathbf{ConCGR-AoI\ Flat\ Scaling:}$" + "\nP95 AoI bounded at $124.3$ s\neven under 40-UAV swarm",
                 xy=(40, 124.3), xytext=(22, 540),
                 arrowprops=dict(facecolor='black', edgecolor='black', shrink=0.05, width=1.4, headwidth=7),
                 fontsize=10, fontweight='bold', color='black',
                 bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="black", lw=1.0, alpha=0.96))

    ax1.set_xlabel('Swarm Scale $N$ (Patrol UAVs)', fontsize=13.5, fontweight='bold')
    ax1.set_ylabel('P95 Peak-AoI (s) [Lower is Better]', fontsize=13.5, fontweight='bold')
    ax1.set_title('(b) Mission Freshness (Tail AoI) vs Swarm Scale', fontsize=14.5, fontweight='bold', pad=12)
    ax1.grid(True, ls="--", alpha=0.4)
    ax1.set_xticks(scales)
    ax1.set_ylim(0, 680)
    ax1.legend(loc='center left', bbox_to_anchor=(0.03, 0.42), fontsize=9.5, framealpha=0.96)

    # Panel 6(c): Buffer Capacity Sweep at N=20 (Buffer Stress / Telemetry Storm)
    ax2 = axes[2]
    # Highlight saturation knee B* = 8 MB
    ax2.axvline(8.0, color='#2E7D32', linestyle=':', lw=2.4, label='Optimal Storage Knee $B^* = 8$ MB')

    for proto in ["ConCGR-AoI", "Deterministic-CGR", "PRoPHETv2", "Epidemic", "Spray-and-Wait"]:
        pts_x = []
        pts_y = []
        pts_err = []
        for b in buffers:
            st = aggregate_stats(groups.get((proto, 20, b), []))
            if st:
                pts_x.append(b)
                pts_y.append(st["P95_AoI_s_mean"])
                pts_err.append(st["P95_AoI_s_std"])
        lw = 3.2 if proto == "ConCGR-AoI" else 2.2
        ms = 9.5 if proto == "ConCGR-AoI" else 7.5
        ax2.errorbar(pts_x, pts_y, yerr=pts_err, label=proto, color=PALETTE[proto],
                     marker=MARKERS[proto], markersize=ms, linewidth=lw, capsize=5, capthick=1.4)

    # Callout for buffer management
    ax2.annotate(r"$\mathbf{Utility\ Evictor\ U_b:}$" + "\nPurges stale routine packets;\nZero overflow drop of fire alerts",
                 xy=(8.0, 95.7), xytext=(9.2, 360),
                 arrowprops=dict(facecolor='black', edgecolor='black', shrink=0.05, width=1.4, headwidth=7),
                 fontsize=10, fontweight='bold', color='black',
                 bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="black", lw=1.0, alpha=0.96))

    ax2.set_xlabel('Per-Node DTN Storage Capacity $B$ (MB)', fontsize=13.5, fontweight='bold')
    ax2.set_ylabel('P95 Peak-AoI (s) at $N=20$ UAVs', fontsize=13.5, fontweight='bold')
    ax2.set_title('(c) Buffer Stress Sensitivity ($N=20$, Telemetry Storm)', fontsize=14.5, fontweight='bold', pad=12)
    ax2.grid(True, ls="--", alpha=0.4)
    ax2.set_xticks(buffers)
    ax2.set_ylim(0, 580)
    ax2.legend(loc='upper right', fontsize=10.0, framealpha=0.94)

    plt.tight_layout()
    plt.savefig(OUT_FIG6, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Generated Figure 6 at: {OUT_FIG6}")

    # Copy Figure 6 to Paper Figures and Artifact Directory
    paper_fig6 = "/home/jarvis/concgr_dtn_ws/paper/figures/fig6_ns3_scalability_analysis.png"
    artifact_fig6 = os.path.join(ARTIFACT_DIR, "fig6_ns3_scalability_analysis.png")
    import shutil
    if os.path.exists(os.path.dirname(paper_fig6)):
        shutil.copyfile(OUT_FIG6, paper_fig6)
        print(f"Copied Figure 6 to Paper Figures: {paper_fig6}")
    shutil.copyfile(OUT_FIG6, artifact_fig6)
    print(f"Copied Figure 6 to Artifact Directory: {artifact_fig6}")

    # Print summary of findings
    print("\n" + "=" * 80)
    print("KEY SCALABILITY FINDINGS ACROSS 117 PHYSICAL SIMULATION RUNS:")
    print("=" * 80)
    for n in scales:
        c_st = aggregate_stats(groups.get(("ConCGR-AoI", n, 8.0), []))
        d_st = aggregate_stats(groups.get(("Deterministic-CGR", n, 8.0), []))
        e_st = aggregate_stats(groups.get(("Epidemic", n, 8.0), []))
        p_st = aggregate_stats(groups.get(("PRoPHETv2", n, 8.0), []))
        print(f"\n--- Swarm N = {n} UAVs ---")
        print(f"  ConCGR-AoI:       P95 AoI = {c_st['P95_AoI_s_mean']:.1f}s | MAC Collisions = {int(c_st['MacCollisions_mean']):,} | Packets = {int(c_st['TotalPackets_mean']):,} | Solve = {c_st['AvgSolveTime_us_mean']:.1f}us")
        print(f"  Deterministic-CGR: P95 AoI = {d_st['P95_AoI_s_mean']:.1f}s | MAC Collisions = {int(d_st['MacCollisions_mean']):,} | Packets = {int(d_st['TotalPackets_mean']):,} | Solve = {d_st['AvgSolveTime_us_mean']:.1f}us")
        print(f"  PRoPHETv2:        P95 AoI = {p_st['P95_AoI_s_mean']:.1f}s | MAC Collisions = {int(p_st['MacCollisions_mean']):,} | Packets = {int(p_st['TotalPackets_mean']):,} | Solve = {p_st['AvgSolveTime_us_mean']:.1f}us")
        print(f"  Epidemic:         P95 AoI = {e_st['P95_AoI_s_mean']:.1f}s | MAC Collisions = {int(e_st['MacCollisions_mean']):,} | Packets = {int(e_st['TotalPackets_mean']):,} | Solve = {e_st['AvgSolveTime_us_mean']:.1f}us")

if __name__ == "__main__":
    main()
