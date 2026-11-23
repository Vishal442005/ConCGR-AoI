#!/usr/bin/env bash
# =============================================================================
# reproduce_all.sh - End-to-End Master Reproduction Script
# Project: ConCGR-AoI (Elsevier Ad Hoc Networks)
# Author: Vishal Chandel
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

echo "======================================================================="
echo "   REPRODUCIBILITY SUITE: ConCGR-AoI Research Artifacts"
echo "======================================================================="

# 1. Colcon build C++ core & ROS 2 packages
if command -v colcon &> /dev/null; then
    echo "[1/6] Building ROS 2 workspace (C++17 Release mode)..."
    colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
    source install/setup.bash || true
else
    echo "[1/6] colcon not found; skipping binary rebuild (using cached artifacts)."
fi

# 2. Run unit tests
echo "[2/6] Running C++17 mathematical verification unit tests..."
python3 scripts/run_unit_tests.py

# 3. Run HIL verification
echo "[3/6] Running ROS 2 HIL disruption recovery verification..."
python3 scripts/run_ros2_hil_verification.py

# 4. Statistical analysis
echo "[4/6] Running statistical significance tests (Wilcoxon signed-rank)..."
python3 scripts/statistical_analysis.py

# 5. Regenerate all 10 publication figures
echo "[5/5] Regenerating publication-quality Figures 1 to 10..."
python3 scripts/generate_figure_1_visual.py
python3 scripts/generate_figures_1_and_2.py
python3 scripts/generate_publication_figures.py
python3 scripts/generate_ns3_scalability_artifacts.py
python3 scripts/generate_figure_7_visual.py
python3 scripts/generate_figures_8_9_10.py

echo "======================================================================="
echo "   REPRODUCTION COMPLETE: All artifacts verified successfully!"
echo "   Publication Figures: data/plots/ (Figures 1 through 10)"
echo "======================================================================="
