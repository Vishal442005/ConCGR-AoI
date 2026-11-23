# ConCGR-AoI: Distribution-Free Conformal Contact-Graph Routing for Real-Time and Fresh Telemetry in Disrupted Flying Ad-Hoc Networks

**Author:** Vishal Chandel  
**Affiliation:** Department of Electronics and Communication Engineering, Jawaharlal Nehru Government Engineering College Sundernagar, Himachal Pradesh, India  
**Target Venue:** *Elsevier Ad Hoc Networks*  

---

## Overview

This repository contains the complete research artifacts, C++17 routing algorithms, ROS 2 bridge nodes, physical ns-3.40 simulation campaigns, and empirical Himalayan flight datasets for **ConCGR-AoI**.

ConCGR-AoI provides finite-sample distribution-free guarantees on telemetry age and contact arrival bounds over highly non-stationary aerial topologies without requiring Gaussian or stationarity assumptions.

---

## Repository Structure

```text
concgr_dtn_ws/
├── src/
│   ├── concgr_core/          # Optimized C++17 Dijkstra Minimax-AoI engine & online ACI
│   ├── concgr_msgs/          # ROS 2 custom message specifications (Bundle, ContactPlan)
│   └── concgr_dtn_bridge/    # Hardware-in-the-loop bridge, publisher & ingestion nodes
├── data/
│   ├── terrain/              # NASA SRTM 30m Kangra Valley DEM topography
│   ├── trajectories/         # Real 50 Hz PX4 quadrotor flight dynamics telemetry
│   ├── wind/                 # Real-world mountainous wind field logs
│   ├── payloads/             # Authentic FLIR radiometric thermal frames & LiDAR PCD submaps
│   └── plots/                # High-resolution publication-quality Figures 1 to 10
├── scripts/
│   ├── run_unit_tests.py     # C++17 mathematical unit test suite
│   ├── run_ros2_hil_verification.py # ROS 2 HIL mountain ridge occlusion testbed
│   ├── run_ns3_campaign.py   # Large-scale 802.11p physical simulation campaign (117 runs)
│   ├── statistical_analysis.py # Paired Wilcoxon signed-rank and Cliff's delta tests
│   └── generate_*.py         # Autonomous figure generation scripts for all paper plots
└── reproduce_all.sh          # Master script executing full build, test, and figure reproduction
```

---

## Quickstart & Reproducibility

### 1. Prerequisites
- **Operating System:** Ubuntu 22.04 LTS / Linux
- **ROS 2:** Humble Hawksbill (or Iron)
- **Compiler:** GCC/G++ supporting C++17
- **Simulator:** ns-3.40 (with IEEE 802.11p CSMA/CA support)
- **Python:** 3.10+ (`numpy`, `scipy`, `pandas`, `matplotlib`, `pillow`)

### 2. Building the ROS 2 / C++17 Workspace
```bash
cd concgr_dtn_ws
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
source install/setup.bash
```

### 3. Running Unit Tests
```bash
python3 scripts/run_unit_tests.py
```

### 4. Running Hardware-in-the-Loop Disruption Verification
```bash
python3 scripts/run_ros2_hil_verification.py
```

### 5. Running ns-3 Scalability Campaign & Statistical Rigor
```bash
python3 scripts/run_ns3_campaign.py
python3 scripts/statistical_analysis.py
```

### 6. Regenerating All Publication Figures (Figures 1–10)
```bash
python3 scripts/generate_figure_1_visual.py
python3 scripts/generate_figures_1_and_2.py
python3 scripts/generate_publication_figures.py
python3 scripts/generate_ns3_scalability_artifacts.py
python3 scripts/generate_figure_7_visual.py
python3 scripts/generate_figures_8_9_10.py
```

---

## One-Click Reproduction

To run the complete pipeline end-to-end:
```bash
chmod +x reproduce_all.sh
./reproduce_all.sh
```

---

## Citation & Zenodo DOI

```bibtex
@software{chandel2026concgr,
  author       = {Vishal Chandel},
  title        = {{ConCGR-AoI: Distribution-Free Conformal Contact-Graph Routing for Real-Time Telemetry in Disrupted Flying Ad-Hoc Networks}},
  year         = {2026},
  publisher    = {Zenodo},
  version      = {v1.0.0},
  url          = {https://github.com/Vishal442005/ConCGR-AoI}
}
```

---

## License

This project is licensed under the MIT License - see the LICENSE file for details.
