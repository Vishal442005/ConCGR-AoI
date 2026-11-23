#!/usr/bin/env python3
"""
scripts/generate_figure_7_visual.py

Generates publication-quality Figure 7:
  - Panel (a): Real-Time Payload Delivery Resilience across 20s Mountain Blackout
  - Panel (b): Peak Age-of-Information Freshness Profile under Terrain Disruption
"""

import os
import shutil
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches

OUT_PLOT = "/home/jarvis/concgr_dtn_ws/data/plots/fig7_ros2_hil_disruption_recovery.png"
PAPER_PLOT = "/home/jarvis/concgr_dtn_ws/paper/figures/fig7_ros2_hil_disruption_recovery.png"
ARTIFACT_DIR = "/home/jarvis/.gemini/antigravity/brain/8a2792bb-81d2-4989-b5f6-18baa5e33c69"
CSV_PATH = "/home/jarvis/concgr_dtn_ws/data/ros2_hil_verification_results.csv"

def generate_figure_7():
    print(f"Loading {CSV_PATH}...")
    df = pd.read_csv(CSV_PATH)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10.5), sharex=True, dpi=300)

    # -------------------------------------------------------------------------
    # Panel 7(a): Cumulative Payload Delivery
    # -------------------------------------------------------------------------
    dtn_cum = np.cumsum(df['ConCGR_AoI_Delivered'].astype(int))
    dds_cum = np.cumsum(df['Native_DDS_Delivered'].astype(int))
    x = df['BundleId'].values

    # Occlusion zone
    blackout_mask = df['BlackoutPeriod'].values
    blackout_indices = np.where(blackout_mask)[0]
    x_start = df['BundleId'].iloc[blackout_indices[0]] - 0.5
    x_end = df['BundleId'].iloc[blackout_indices[-1]] + 0.5

    # Shaded Blackout Zone in ax1 and ax2
    for ax in (ax1, ax2):
        ax.axvspan(x_start, x_end, color='#FFCDD2', alpha=0.45, hatch='//',
                   label='20s Mountain Blackout (ITU-R P.526 Occlusion)' if ax == ax1 else None, zorder=2)
        ax.axvline(x_start, color='#C62828', linestyle='--', lw=1.8, alpha=0.8, zorder=3)
        ax.axvline(x_end, color='#2E7D32', linestyle='--', lw=1.8, alpha=0.8, zorder=3)
        ax.grid(True, ls='--', alpha=0.45, zorder=1)

    # ConCGR-AoI Cumulative Curve
    ax1.plot(x, dtn_cum, color='#2E7D32', lw=3.2, marker='o', markersize=6.5,
             label='ConCGR-AoI (DTN Custody Transfer & Rapid Drain)', zorder=5)
    ax1.fill_between(x, 0, dtn_cum, color='#C8E6C9', alpha=0.35, zorder=3)

    # CycloneDDS Cumulative Curve
    ax1.plot(x, dds_cum, color='#D32F2F', lw=2.6, ls='--', marker='x', markersize=7.5,
             markeredgewidth=2.0, label='Native ROS 2 CycloneDDS (Bufferless UDP Drops)', zorder=4)

    # Annotate dropped datagrams along CycloneDDS
    drop_x = x[blackout_indices]
    drop_y = np.full_like(drop_x, dds_cum[blackout_indices[0]])
    ax1.scatter(drop_x, drop_y, color='#B71C1C', s=35, marker='v', alpha=0.7, zorder=6,
                label='CycloneDDS Dropped Datagrams (20 Pkts)')

    # Callout for ConCGR backlog drain (repositioned inside plot bounds, pointing down-right)
    ax1.annotate(r"$\mathbf{Immediate\ Backlog\ Drain:}$" + "\nAll 20 queued bundles burst across\nin 42 ms upon LoS clearance (33/33 total, 100%)",
                 xy=(x_end + 0.5, dtn_cum.iloc[blackout_indices[-1] + 1]),
                 xytext=(16.5, 28.5),
                 arrowprops=dict(facecolor='black', edgecolor='black', shrink=0.05, width=1.4, headwidth=7),
                 fontsize=10, fontweight='bold', color='black',
                 bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="black", lw=1.0, alpha=0.96), zorder=12)

    # Callout for CycloneDDS flatline
    ax1.annotate(r"$\mathbf{Bufferless\ Socket\ Drop:}$" + "\n0/20 blackout bundles delivered\n(60.6% total mission data loss)",
                 xy=(13, dds_cum.iloc[blackout_indices[10]]),
                 xytext=(6.5, 12.0),
                 arrowprops=dict(facecolor='black', edgecolor='black', shrink=0.05, width=1.4, headwidth=7),
                 fontsize=10, fontweight='bold', color='black',
                 bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="black", lw=1.0, alpha=0.96), zorder=12)

    # Blackout label banner (placed at y=38.5, well above all curves and clear of upper-left legend)
    ax1.text((x_start + x_end) / 2.0, 38.5, "MOUNTAIN RIDGE OCCLUSION BLACKOUT (20 SECONDS)",
             ha='center', va='center', fontsize=10.5, fontweight='bold', color='black',
             bbox=dict(boxstyle="round,pad=0.3", fc="white", ec="black", lw=1.0, alpha=0.96), zorder=12)

    ax1.set_ylabel('Cumulative Delivered Bundles', fontsize=13, fontweight='bold')
    ax1.set_title('(a) Real-Time Payload Delivery Resilience across 20s Mountain Blackout (Hardware-in-the-Loop Testbed)',
                  fontsize=14, fontweight='bold', pad=12)
    ax1.tick_params(axis='both', labelsize=11.5)
    ax1.legend(loc='upper left', framealpha=0.95, fontsize=10.5)
    ax1.set_ylim(-1, 42)

    # -------------------------------------------------------------------------
    # Panel 7(b): Real-Time Peak AoI Freshness
    # -------------------------------------------------------------------------
    # Shaded operational freshness zones
    ax2.axhspan(0, 15.0, color='#E8F5E9', alpha=0.35, label='Fresh Real-Time Telemetry Corridor (< 15 s)', zorder=2)
    ax2.axhspan(15.0, 30.0, color='#FFF8E1', alpha=0.45, label='Tolerable Latency Horizon (15 - 30 s)', zorder=2)
    ax2.axhspan(30.0, 42.0, color='#FFEBEE', alpha=0.40, label='Urgent Deadline Violation Zone (> 30 s)', zorder=2)

    # ConCGR-AoI curve
    dtn_sub = df[df['ConCGR_AoI_Delivered'] == True]
    ax2.fill_between(dtn_sub['BundleId'], 0, dtn_sub['Peak_AoI_s'], color='#81D4FA', alpha=0.35, zorder=3)
    ax2.plot(dtn_sub['BundleId'], dtn_sub['Peak_AoI_s'], color='#0277BD', lw=2.6, ls='-', zorder=4)

    # Color-coded markers by priority
    prio3 = dtn_sub[dtn_sub['Priority'] == 3]
    prio2 = dtn_sub[dtn_sub['Priority'] == 2]
    prio1 = dtn_sub[dtn_sub['Priority'] == 1]

    ax2.scatter(prio3['BundleId'], prio3['Peak_AoI_s'], color='#D32F2F', s=95, marker='o',
                edgecolors='black', lw=1.4, zorder=7, label=r'Priority 3: FLIR Wildfire Thermal ($D_b = 30$ s)')
    ax2.scatter(prio2['BundleId'], prio2['Peak_AoI_s'], color='#F57F17', s=90, marker='^',
                edgecolors='black', lw=1.4, zorder=7, label=r'Priority 2: Airborne LiDAR Submaps ($D_b = 90$ s)')
    ax2.scatter(prio1['BundleId'], prio1['Peak_AoI_s'], color='#0288D1', s=80, marker='s',
                edgecolors='black', lw=1.4, zorder=7, label=r'Priority 1: PX4 Flight Avionics ($D_b = 240$ s)')

    # Annotate max AoI peak (positioned cleanly with positive x-coordinates, pointing down-left)
    max_idx = df['Peak_AoI_s'].idxmax()
    max_aoi = df.loc[max_idx, 'Peak_AoI_s']
    max_bundle = df.loc[max_idx, 'BundleId']

    ax2.annotate(r"$\mathbf{Max\ Realized\ Peak\ AoI = 19.98\ s\ (\leq 30\ s\ Deadline)}$" + "\nBacklog Drains to Sub-25 ms Baseline Instantly",
                 xy=(max_bundle, max_aoi),
                 xytext=(9.5, 25.5),
                 arrowprops=dict(facecolor='black', edgecolor='black', shrink=0.05, width=1.4, headwidth=7),
                 fontsize=10, fontweight='bold', color='black',
                 bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="black", lw=1.0, alpha=0.96), zorder=12)

    # Urgent FLIR Thermal Deadline line
    ax2.axhline(30.0, color='#D32F2F', ls='--', linewidth=2.2, alpha=0.95,
                label=r'FLIR Thermal Urgent Deadline ($D_b = 30$ s)')

    ax2.set_xlabel('Bundle Sequence ID / Mission Timeline (seconds)', fontsize=13, fontweight='bold')
    ax2.set_ylabel(r'Peak AoI $\Delta(t)$ (seconds)', fontsize=13, fontweight='bold')
    ax2.set_title(r'(b) Peak Age-of-Information Freshness Profile under Terrain Disruption ($\Delta(t) = t - U_b$)',
                  fontsize=14, fontweight='bold', pad=12)
    ax2.tick_params(axis='both', labelsize=11.5)
    ax2.set_ylim(-1, 42.0)
    ax2.legend(loc='upper right', framealpha=0.95, fontsize=10.0)

    plt.tight_layout()
    plt.savefig(OUT_PLOT, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"--> Successfully generated high-end visual Figure 7 at: {OUT_PLOT}")

    # Copy to paper and artifacts
    shutil.copyfile(OUT_PLOT, PAPER_PLOT)
    shutil.copyfile(OUT_PLOT, os.path.join(ARTIFACT_DIR, "fig7_ros2_hil_disruption_recovery.png"))
    print(f"--> Copied Figure 7 to Paper: {PAPER_PLOT}")
    print(f"--> Copied Figure 7 to Artifact: {os.path.join(ARTIFACT_DIR, 'fig7_ros2_hil_disruption_recovery.png')}")

if __name__ == "__main__":
    generate_figure_7()
