#include "concgr_core/aoi_buffer_evictor.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Running Test: AoI Utility Buffer Evictor (Eq. 12 Verification) ===" << std::endl;

    concgr::AoiBufferEvictor::Config cfg;
    cfg.max_buffer_bytes = 500 * 1024; // 500 KB small test buffer limit
    concgr::AoiBufferEvictor evictor(cfg);

    std::vector<concgr::Bundle> queue;
    double now = 100.0;

    // Bundle 1: High priority Thermal alert (priority=3, size=30 KB, gen=95s, age=5s)
    concgr::Bundle b1;
    b1.id = 1; b1.priority = 3; b1.t_gen = 95.0; b1.size_bytes = 30 * 1024;

    // Bundle 2: Old stale telemetry (priority=1, size=200 KB, gen=10s, age=90s)
    concgr::Bundle b2;
    b2.id = 2; b2.priority = 1; b2.t_gen = 10.0; b2.size_bytes = 200 * 1024;

    // Bundle 3: Medium priority Map (priority=2, size=250 KB, gen=80s, age=20s)
    concgr::Bundle b3;
    b3.id = 3; b3.priority = 2; b3.t_gen = 80.0; b3.size_bytes = 250 * 1024;

    evictor.admit_bundle(queue, b1, now);
    evictor.admit_bundle(queue, b2, now);
    evictor.admit_bundle(queue, b3, now);

    std::cout << "Current queue size: " << queue.size() << " bundles, total bytes: "
              << evictor.get_current_bytes() / 1024 << " KB / "
              << evictor.get_max_bytes() / 1024 << " KB" << std::endl;

    // Now incoming Bundle 4: Fresh High Priority Thermal Alert (priority=3, size=100 KB, gen=99s, age=1s)
    // Queue is currently 30 + 200 + 250 = 480 KB. Adding 100 KB would exceed 500 KB.
    // The evictor MUST evict Bundle 2 (stale telemetry, age=90s, lowest utility)!
    concgr::Bundle b4;
    b4.id = 4; b4.priority = 3; b4.t_gen = 99.0; b4.size_bytes = 100 * 1024;

    auto evicted = evictor.admit_bundle(queue, b4, now);

    std::cout << "Evicted bundle IDs: ";
    for (auto id : evicted) std::cout << id << " ";
    std::cout << std::endl;

    assert(!evicted.empty());
    assert(evicted[0] == 2); // Bundle 2 must be evicted!
    assert(evictor.get_current_bytes() <= cfg.max_buffer_bytes);

    std::cout << "[PASS] AoI-Utility buffer eviction correctly evicted stale telemetry while protecting fresh alerts!" << std::endl;
    return 0;
}
