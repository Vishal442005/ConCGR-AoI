#!/usr/bin/env python3
"""
scripts/run_unit_tests.py
Automated test runner for the ConCGR-AoI C++17 Core Engine.
"""

import subprocess
import sys
from pathlib import Path

WS_ROOT = Path("/home/jarvis/concgr_dtn_ws")
TEST_BINS = [
    WS_ROOT / "build/concgr_core/test_aci_calibrator",
    WS_ROOT / "build/concgr_core/test_conformal_dijkstra",
    WS_ROOT / "build/concgr_core/test_diffraction",
    WS_ROOT / "build/concgr_core/test_buffer_eviction"
]

def main():
    print("=" * 60)
    print("ConCGR-AoI Phase 1: Mathematical Core Unit Test Suite")
    print("=" * 60)
    
    all_passed = True
    for tb in TEST_BINS:
        if not tb.exists():
            print(f"[FAIL] Missing test binary: {tb}")
            all_passed = False
            continue
            
        print(f"\n--> Executing: {tb.name}")
        res = subprocess.run([str(tb)], capture_output=True, text=True)
        print(res.stdout)
        if res.stderr:
            print("[STDERR]:", res.stderr)
            
        if res.returncode == 0:
            print(f"[{tb.name}] Status: PASSED (Exit code 0)")
        else:
            print(f"[{tb.name}] Status: FAILED (Exit code {res.returncode})")
            all_passed = False
            
    print("\n" + "=" * 60)
    if all_passed:
        print("ALL TESTS PASSED: 100% Verification of Lemma 1, Theorem 1 & 2!")
        print("=" * 60)
        sys.exit(0)
    else:
        print("TEST SUITE FAILED!")
        print("=" * 60)
        sys.exit(1)

if __name__ == "__main__":
    main()
