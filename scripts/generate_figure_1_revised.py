#!/usr/bin/env python3
"""
generate_figure_1_revised.py

Generates an exhaustive, publication-grade Figure 1 with large, crisp typography:
  - Panel (a): Operational Scenario & Physical Disruption Environment in Mountain Terrain
               (Patrol UAV with FLIR & LiDAR, Wind Turbulence, Knife-Edge Ridge Occlusion, Relay UAV, Ground Sink)
  - Panel (b): ConCGR-AoI End-to-End System & Algorithmic Architecture
               (Sensor Ingestion, ACI Calibrator, Minimax Dijkstra, Quota Controller, Utility Evictor, ROS 2 BPv7)
  - Panel (c): Conformal Contact Horizon vs. Classical Deterministic CGR Failure under Wind Drift
"""

import os
import shutil
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.path import Path

OUT_DIR = "/home/jarvis/concgr_dtn_ws/data/plots"
PAPER_FIG_DIR = "/home/jarvis/concgr_dtn_ws/paper/figures"
ARTIFACT_DIR = "/home/jarvis/.gemini/antigravity/brain/8a2792bb-81d2-4989-b5f6-18baa5e33c69"

os.makedirs(OUT_DIR, exist_ok=True)
os.makedirs(PAPER_FIG_DIR, exist_ok=True)

# Colorblind-friendly palette
COLORS = {
    'sky_blue': '#E1F5FE',
    'deep_blue': '#0277BD',
    'mountain_brown': '#795548',
    'mountain_dark': '#4E342E',
    'mountain_light': '#D7CCC8',
    'uav_body': '#263238',
    'uav_rotor': '#546E7A',
    'thermal_red': '#D32F2F',
    'lidar_green': '#2E7D32',
    'wind_cyan': '#00838F',
    'gold': '#F57F17',
    'engine_bg': '#FFFDE7',
    'engine_border': '#FBC02D',
    'block_bg': '#FFFFFF',
    'dtn_green': '#E8F5E9',
    'dtn_border': '#388E3C',
    'accent_orange': '#E65100',
    'purple': '#6A1B9A'
}

def draw_uav(ax, x, y, size=1.1, label=""):
    """Draws a detailed quadrotor glyph with airframe, arms, and rotors."""
    # Arms
    ax.plot([x - 1.8*size, x + 1.8*size], [y - 0.6*size, y + 0.6*size], color=COLORS['uav_body'], lw=2.4*size, zorder=10)
    ax.plot([x - 1.8*size, x + 1.8*size], [y + 0.6*size, y - 0.6*size], color=COLORS['uav_body'], lw=2.4*size, zorder=10)
    # Central fuselage
    central = patches.Circle((x, y), 0.9*size, fc=COLORS['uav_body'], ec='black', lw=1.3, zorder=11)
    ax.add_patch(central)
    # Rotors
    for rx, ry in [(x - 1.8*size, y - 0.6*size), (x + 1.8*size, y + 0.6*size),
                   (x - 1.8*size, y + 0.6*size), (x + 1.8*size, y - 0.6*size)]:
        rotor = patches.Ellipse((rx, ry), 1.6*size, 0.45*size, fc=COLORS['uav_rotor'], ec='black', lw=0.9, alpha=0.85, zorder=12)
        ax.add_patch(rotor)
    # Antenna
    ax.plot([x, x], [y, y + 1.5*size], color='black', lw=1.6, zorder=11)
    ax.scatter([x], [y + 1.5*size], color='red', s=24*size, zorder=12)
    # Label
    if label:
        ax.text(x, y + 2.5*size, label, ha='center', va='bottom', fontsize=11, fontweight='bold',
                bbox=dict(boxstyle="round,pad=0.25", fc="white", ec=COLORS['deep_blue'], lw=1.2, alpha=0.95), zorder=15)

def generate_figure_1():
    print("Generating High-Fidelity Figure 1 with Increased Text Sizes...")
    fig = plt.figure(figsize=(16, 12.5), dpi=300)
    
    # 3-row layout:
    ax1 = plt.subplot2grid((3, 1), (0, 0), rowspan=1)
    ax2 = plt.subplot2grid((3, 1), (1, 0), rowspan=1)
    ax3 = plt.subplot2grid((3, 1), (2, 0), rowspan=1)

    # =========================================================================
    # PANEL (a): OPERATIONAL SCENARIO & PHYSICAL DISRUPTION ENVIRONMENT
    # =========================================================================
    ax1.set_xlim(0, 100)
    ax1.set_ylim(0, 50)
    ax1.axis('off')
    ax1.set_title("(a) Disrupted Flying Ad-Hoc Network (FANET) Operating Scenario in Rugged Himalayan Topography (Kangra Valley)",
                  fontsize=13.5, fontweight='bold', pad=10, loc='left')

    # Sky background
    sky = patches.Rectangle((0, 0), 100, 50, fc=COLORS['sky_blue'], ec='none', zorder=0)
    ax1.add_patch(sky)

    # Mountain Topography Profile (Elevation curve modeling real Kangra 1104m relief)
    x_terrain = np.linspace(0, 100, 400)
    y_terrain = (10.0 + 16.0 * np.exp(-((x_terrain - 48)**2) / 120.0)
                 + 8.0 * np.exp(-((x_terrain - 20)**2) / 80.0)
                 + 4.0 * np.sin(x_terrain / 3.0))
    y_terrain[y_terrain < 4.0] = 4.0

    ax1.fill_between(x_terrain, 0, y_terrain, color=COLORS['mountain_brown'], alpha=0.85, zorder=2)
    ax1.plot(x_terrain, y_terrain, color=COLORS['mountain_dark'], lw=2.4, zorder=3)
    
    # Mountain ridge apex label
    ax1.text(48, 28, "Kangra Mountain Ridge Apex\n(Relief $\\Delta h = 1,104$ m)", ha='center', va='bottom',
             fontsize=11, fontweight='bold', color=COLORS['mountain_dark'],
             bbox=dict(boxstyle="round,pad=0.2", fc="#EFEBE9", ec=COLORS['mountain_dark'], alpha=0.9))

    # Wildfire Ground Front (detected by UAV 1)
    for wx, wy in [(12, 10), (14, 11), (16, 9), (18, 12)]:
        flame = patches.Polygon([[wx, wy], [wx+1.2, wy+3.5], [wx+2.4, wy]], closed=True, fc='orange', ec='red', lw=1, zorder=4)
        ax1.add_patch(flame)
    ax1.text(15, 6, "Active Wildfire Front\n($T_{\\mathrm{pixel}} > 650$ K)", ha='center', va='center',
             fontsize=10, fontweight='bold', color='red', bbox=dict(boxstyle="round,pad=0.25", fc="yellow", ec="red", lw=1.2, alpha=0.95), zorder=5)

    # UAV 1: Patrol Reconnaissance UAV
    draw_uav(ax1, 16, 36, size=1.1, label="Patrol UAV 1 (Source)")
    
    # Thermal FOV Cone
    fov_thermal = patches.Polygon([[16, 36], [10, 12], [22, 12]], closed=True, fc=COLORS['thermal_red'], alpha=0.18, ec=COLORS['thermal_red'], ls='--', lw=1.3, zorder=3)
    ax1.add_patch(fov_thermal)
    ax1.text(16, 23.5, "FLIR Thermal & LiDAR FOV\n($D_b = 30$ s / $90$ s)", ha='center', va='center', fontsize=9.5, fontweight='bold', color=COLORS['thermal_red'])

    # Wind Gust Vector (Turbulence)
    for wy in [39, 42, 45]:
        ax1.annotate("", xy=(25, wy), xytext=(12, wy),
                     arrowprops=dict(arrowstyle="->", lw=2.2, color=COLORS['wind_cyan'], ls="-"))
    ax1.text(18, 47, "Turbulent Wind Gusts $\\mathbf{w}(t) \\leq 18.2$ m/s\n(Non-Stationary Trajectory Drift)",
             ha='center', va='center', fontsize=10.5, fontweight='bold', color=COLORS['wind_cyan'])

    # Ground Station Sink (Node 0)
    sink_box = patches.Rectangle((87, 4), 11, 8, fc='#37474F', ec='black', lw=1.6, zorder=5)
    ax1.add_patch(sink_box)
    ax1.plot([92.5, 92.5], [12, 18], color='black', lw=2.2, zorder=6)
    ax1.plot([89, 96], [18, 18], color='black', lw=2.2, zorder=6)
    ax1.text(92.5, 21.5, "Ground Station Sink\n(Node 0, Base Camp)", ha='center', va='center', fontsize=11, fontweight='bold',
             bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="black", lw=1.2, alpha=0.95), zorder=7)

    # Line of Sight Occlusion Ray (Blocked by Ridge)
    ax1.plot([16, 92.5], [36, 15], color='red', lw=2.4, ls=':', zorder=4)
    # Red X at obstacle apex
    ax1.scatter([48], [26.5], marker='X', color='red', s=180, zorder=10)
    ax1.text(48, 21.5, "DIRECT LoS BLOCKED\nITU-R P.526 Loss $> 35$ dB\n(Native CycloneDDS Drops 100%)",
             ha='center', va='top', fontsize=10, fontweight='bold', color='red',
             bbox=dict(boxstyle="round,pad=0.3", fc="#FFEBEE", ec="red", lw=1.2, alpha=0.95), zorder=12)

    # UAV 2: Aerial Ridge Relay (Bypasses occlusion)
    draw_uav(ax1, 52, 42.5, size=1.1, label="Ridge Relay UAV 2\n(DTN Custody Store)")

    # Relay Hop 1 (UAV 1 -> UAV 2)
    ax1.annotate("", xy=(50, 42.5), xytext=(18, 37.5),
                 arrowprops=dict(arrowstyle="->", lw=2.4, color=COLORS['deep_blue'], ls="-"))
    ax1.text(32, 42, "Conformal Hop 1: BPv7 Custody\n($Q_b=2$, Rate $=6$ Mbps)", ha='center', va='center',
             fontsize=10, fontweight='bold', color=COLORS['deep_blue'])

    # Relay Hop 2 (UAV 2 -> Ground Sink)
    ax1.annotate("", xy=(90.5, 19.5), xytext=(54, 41.5),
                 arrowprops=dict(arrowstyle="->", lw=2.4, color=COLORS['dtn_border'], ls="-"))
    ax1.text(76, 32.5, "Conformal Hop 2: Clear Downlink\n(Drains Occlusion Backlog)", ha='center', va='center',
             fontsize=10, fontweight='bold', color=COLORS['dtn_border'])

    # Store-Carry-Forward Buffer Callout
    ax1.text(52, 33.5, "Store-Carry-Forward Custody Buffer\n(Protected by Utility Evictor $U_b$)", ha='center', va='center',
             fontsize=9.5, fontweight='bold', color=COLORS['mountain_dark'],
             bbox=dict(boxstyle="round,pad=0.25", fc="#FFF9C4", ec=COLORS['gold'], lw=1.2, alpha=0.95), zorder=12)

    # =========================================================================
    # PANEL (b): CONCGR-AOI END-TO-END AUTONOMOUS SOFTWARE ARCHITECTURE
    # =========================================================================
    ax2.set_xlim(0, 100)
    ax2.set_ylim(0, 36)
    ax2.axis('off')
    ax2.set_title("(b) ConCGR-AoI Autonomous Companion Computer Software Architecture & Mathematical Engine (C++17 / ROS 2)",
                  fontsize=13.5, fontweight='bold', pad=10, loc='left')

    # Background frame
    frame_b = patches.FancyBboxPatch((1, 1), 98, 33, boxstyle="round,pad=0.4", fc="#FAFAFA", ec="#B0BEC5", lw=1.6)
    ax2.add_patch(frame_b)

    # Section 1: Ingestion
    s1 = patches.FancyBboxPatch((2.5, 2.5), 21, 30, boxstyle="round,pad=0.3", fc="#E3F2FD", ec=COLORS['deep_blue'], lw=1.8)
    ax2.add_patch(s1)
    ax2.text(13, 29.5, "Multi-Modal Ingestion\n(Heterogeneous Payloads)", ha='center', va='center', fontsize=11, fontweight='bold', color=COLORS['deep_blue'])
    ax2.text(13, 17.5, "• FLIR Wildfire Thermal\n  $D_b = 30$ s, Prio 3 (Urgent)\n  Size: $12 - 34$ KB\n\n• PCL Airborne LiDAR\n  $D_b = 90$ s, Prio 2 (High)\n  Size: $87 - 366$ KB\n\n• 50 Hz PX4 Telemetry\n  $D_b = 240$ s, Prio 1\n  Size: $5.12$ KB",
             ha='center', va='center', fontsize=9.5, color='#0D47A1')

    # Arrow S1 -> S2
    ax2.annotate("", xy=(24.5, 17.5), xytext=(23.5, 17.5), arrowprops=dict(arrowstyle="->", lw=2.4, color=COLORS['deep_blue']))

    # Section 2: ConCGR-AoI Mathematical Engine
    s2 = patches.FancyBboxPatch((25.5, 2), 49, 31, boxstyle="round,pad=0.4", fc=COLORS['engine_bg'], ec=COLORS['gold'], lw=2.2)
    ax2.add_patch(s2)
    ax2.text(50, 30.5, "ConCGR-AoI Mathematical Core Engine (C++17 Static Library)", ha='center', va='center', fontsize=12, fontweight='bold', color=COLORS['accent_orange'])

    # 4 Sub-blocks inside Engine
    # Block 2A: Online ACI Calibrator
    b2a = patches.Rectangle((27, 16.5), 22, 11, fc='white', ec=COLORS['gold'], lw=1.3)
    ax2.add_patch(b2a)
    ax2.text(38, 24.8, "Online ACI Calibrator\n(Lemma 1)", ha='center', va='center', fontsize=10, fontweight='bold', color=COLORS['accent_orange'])
    ax2.text(38, 19.3, "$\\hat{q}_{t+1} = \\hat{q}_t + \\gamma (\\alpha_{\\mathrm{path}} - I_t)$\nGuarantees $\\geq 95\\%$ Coverage",
             ha='center', va='center', fontsize=9.2)

    # Block 2B: Risk-Constrained Replication Quota
    b2b = patches.Rectangle((51, 16.5), 22, 11, fc='white', ec=COLORS['gold'], lw=1.3)
    ax2.add_patch(b2b)
    ax2.text(62, 24.8, "Risk-Bound Quota $Q_b^*$\n(Theorem 1)", ha='center', va='center', fontsize=10, fontweight='bold', color=COLORS['accent_orange'])
    ax2.text(62, 19.3, "$Q_b^* = \\lceil \\frac{\\ln \\alpha_{\\mathrm{path}}}{\\ln (1 - \\hat{P}_{\\mathrm{min}})} \\rceil$\nCaps Replicas: $Q_b \\in [1, 4]$",
             ha='center', va='center', fontsize=9.2)

    # Block 2C: Time-Expanded Minimax AoI Dijkstra
    b2c = patches.Rectangle((27, 4), 22, 11, fc='white', ec=COLORS['gold'], lw=1.3)
    ax2.add_patch(b2c)
    ax2.text(38, 12, "Minimax AoI Dijkstra\n(Theorem 2)", ha='center', va='center', fontsize=10, fontweight='bold', color=COLORS['accent_orange'])
    ax2.text(38, 6.8, "$\\min_P \\max_{e \\in P} \\Delta_b(e)$\nTime-Expanded Graph $\\mathcal{G}_T$\nLatency: $< 75.5\\ \\mu$s",
             ha='center', va='center', fontsize=9.0)

    # Block 2D: Age-Penalty Utility Evictor
    b2d = patches.Rectangle((51, 4), 22, 11, fc='white', ec=COLORS['gold'], lw=1.3)
    ax2.add_patch(b2d)
    ax2.text(62, 12, "Age-Penalty Evictor\n(Utility Buffer Management)", ha='center', va='center', fontsize=10, fontweight='bold', color=COLORS['accent_orange'])
    ax2.text(62, 6.8, "$U_b = \\frac{w(p_b)}{\\mathrm{size}(b) \\cdot (\\tau - t_{\\mathrm{gen}} + \\epsilon)}$\nEvicts Stale Routine Packets",
             ha='center', va='center', fontsize=9.0)

    # Arrow S2 -> S3
    ax2.annotate("", xy=(75.5, 17.5), xytext=(74.5, 17.5), arrowprops=dict(arrowstyle="->", lw=2.4, color=COLORS['accent_orange']))

    # Section 3: ROS 2 Humble Hardware Bridge
    s3 = patches.FancyBboxPatch((76.5, 2.5), 21.5, 30, boxstyle="round,pad=0.3", fc=COLORS['dtn_green'], ec=COLORS['dtn_border'], lw=1.8)
    ax2.add_patch(s3)
    ax2.text(87.2, 29.5, "ROS 2 Humble\nDTN Bridge", ha='center', va='center', fontsize=11, fontweight='bold', color=COLORS['dtn_border'])
    ax2.text(87.2, 17.5, "• RFC 9171 BPv7 Parser\n• $200\\ \\mu$s Micro-Pacing\n• Socket Buffers ($8$ MB)\n• IEEE 802.11p OCB 5.9GHz\n• CSMA/CA Contention\n• Zero-Loss Custody Drain",
             ha='center', va='center', fontsize=9.5, color='#1B5E20')

    # =========================================================================
    # PANEL (c): CONFORMAL CONTACT CALIBRATION VS DETERMINISTIC CGR FAILURE
    # =========================================================================
    ax3.set_xlim(0, 100)
    ax3.set_ylim(0, 24)
    ax3.set_yticks([5, 17])
    ax3.set_yticklabels(["Deterministic CGR\n(Nominal Schedule)", "ConCGR-AoI\n(Adaptive Conformal)"], fontsize=11, fontweight='bold')
    ax3.set_xlabel("Contact Schedule Time Horizon (seconds)", fontsize=12, fontweight='bold')
    ax3.set_title("(c) Temporal Impact of Mountain Wind Turbulence: Classical Deterministic CGR Failure vs. Conformal Contact Expansion",
                  fontsize=13.5, fontweight='bold', pad=10, loc='left')
    ax3.grid(True, ls='--', alpha=0.4, axis='x')
    ax3.tick_params(axis='x', labelsize=11)

    # Deterministic CGR (Lower Bar): Fixed nominal schedule [t=20 to 50]
    ax3.barh(5, 30, left=20, height=5, color='#B0BEC5', edgecolor='black', lw=1.3, label='Nominal Pre-Planned Contact Window')
    # Actual arrival delayed by wind drift [t=38 to 68]
    ax3.barh(5, 30, left=38, height=2.2, color='#D32F2F', alpha=0.85, edgecolor='black', lw=1.3, label='Actual UAV Arrival (Delayed by Wind Gusts)')
    
    # Overlap failure annotation
    ax3.annotate("CONTACT WINDOW COLLAPSE\nDeterministic Miss: 60% Lost Contact\n(Packets Dropped / Stale)",
                 xy=(50, 5), xytext=(63, 6.5),
                 arrowprops=dict(arrowstyle="->", lw=1.8, color='red'),
                 fontsize=10, fontweight='bold', color='red',
                 bbox=dict(boxstyle="round,pad=0.25", fc="#FFEBEE", ec="red", lw=1.1))

    # ConCGR-AoI (Upper Bar): Nominal schedule + Conformal quantile expansion
    ax3.barh(17, 30, left=20, height=5, color=COLORS['deep_blue'], alpha=0.7, edgecolor='black', lw=1.3)
    # Conformal expansion [t=50 to 68]
    ax3.barh(17, 18, left=50, height=5, color=COLORS['gold'], alpha=0.75, hatch='//', edgecolor=COLORS['accent_orange'], lw=1.3,
             label='Conformal Expansion Interval $\\hat{C}_{1-\\alpha_{\\mathrm{path}}}$')
    # Actual arrival falls safely inside conformal interval
    ax3.barh(17, 30, left=38, height=2.2, color=COLORS['dtn_border'], alpha=0.9, edgecolor='black', lw=1.3,
             label='Safe Contact Ingestion (Guaranteed $1-\\alpha_{\\mathrm{path}} \\geq 95\\%$)')

    ax3.annotate("ROBUST CONTACT CAPTURE\nFinite-Sample Guaranteed Coverage\n(Zero Losses across Occlusions)",
                 xy=(56, 17), xytext=(63, 18.5),
                 arrowprops=dict(arrowstyle="->", lw=1.8, color=COLORS['dtn_border']),
                 fontsize=10, fontweight='bold', color=COLORS['dtn_border'],
                 bbox=dict(boxstyle="round,pad=0.25", fc="#E8F5E9", ec=COLORS['dtn_border'], lw=1.1))

    ax3.legend(loc='lower right', fontsize=10, framealpha=0.95, ncol=2)

    plt.tight_layout()
    out_path = os.path.join(OUT_DIR, "fig1_concgr_architecture.png")
    plt.savefig(out_path, dpi=300, bbox_inches="tight")
    plt.close()

    shutil.copyfile(out_path, os.path.join(PAPER_FIG_DIR, "fig1_concgr_architecture.png"))
    shutil.copyfile(out_path, os.path.join(ARTIFACT_DIR, "fig1_concgr_architecture.png"))
    print(f"Generated Revised Figure 1 at: {out_path}")

if __name__ == "__main__":
    generate_figure_1()
