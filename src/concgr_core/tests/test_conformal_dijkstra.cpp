#include "concgr_core/conformal_dijkstra.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Running Test: Conformal Dijkstra (Theorem 1 & 2 Verification) ===" << std::endl;

    concgr::ConformalDijkstraRouter::Config cfg;
    cfg.max_hops = 4;
    cfg.alpha_path = 0.05;
    cfg.k_max = 3;

    concgr::ConformalDijkstraRouter router(cfg);

    // Create a 3-hop store-carry-forward topology:
    // UAV 1 (src) -> UAV 2 -> UAV 3 -> UAV 0 (sink/dst)
    // Contact 1: 1 -> 2 at [10.0, 30.0]s, 6 Mbps
    // Contact 2: 2 -> 3 at [40.0, 70.0]s, 6 Mbps (10s store-carry delay)
    // Contact 3: 3 -> 0 at [80.0, 110.0]s, 6 Mbps (10s store-carry delay)
    // Alternative slow path: 1 -> 0 directly at [200.0, 250.0]s

    std::vector<concgr::ConformalContact> plan;

    concgr::ConformalContact c1;
    c1.id = 1; c1.tx_id = 1; c1.rx_id = 2;
    c1.t_start_conf = 10.0; c1.t_end_conf = 30.0;
    c1.effective_rate_bps = 6.0e6;
    c1.effective_volume_bits = 120.0e6;
    c1.is_admitted = true;
    plan.push_back(c1);

    concgr::ConformalContact c2;
    c2.id = 2; c2.tx_id = 2; c2.rx_id = 3;
    c2.t_start_conf = 40.0; c2.t_end_conf = 70.0;
    c2.effective_rate_bps = 6.0e6;
    c2.effective_volume_bits = 180.0e6;
    c2.is_admitted = true;
    plan.push_back(c2);

    concgr::ConformalContact c3;
    c3.id = 3; c3.tx_id = 3; c3.rx_id = 0;
    c3.t_start_conf = 80.0; c3.t_end_conf = 110.0;
    c3.effective_rate_bps = 6.0e6;
    c3.effective_volume_bits = 180.0e6;
    c3.is_admitted = true;
    plan.push_back(c3);

    // Direct but delayed contact
    concgr::ConformalContact c_direct;
    c_direct.id = 4; c_direct.tx_id = 1; c_direct.rx_id = 0;
    c_direct.t_start_conf = 200.0; c_direct.t_end_conf = 250.0;
    c_direct.effective_rate_bps = 6.0e6;
    c_direct.effective_volume_bits = 300.0e6;
    c_direct.is_admitted = true;
    plan.push_back(c_direct);

    // Bundle generated at t = 5.0 s with 100 s deadline (Thermal alert: size = 30 KB)
    concgr::Bundle bundle;
    bundle.id = 1001;
    bundle.src = 1;
    bundle.dst = 0;
    bundle.priority = 1;
    bundle.t_gen = 5.0;
    bundle.deadline_sec = 100.0;
    bundle.size_bytes = 30 * 1024; // 30 KB = 240,000 bits

    concgr::RoutePlan route = router.compute_route(1, 0, bundle, plan, 5.0);

    assert(route.found);
    std::cout << "Route found! Number of hops: " << route.hops.size() << std::endl;
    for (size_t i = 0; i < route.hops.size(); ++i) {
        const auto& h = route.hops[i];
        std::cout << "  Hop " << i+1 << ": Node " << h.tx_id << " -> " << h.rx_id
                  << " (tx: [" << h.t_tx_start << ", " << h.t_tx_end << "] s)" << std::endl;
    }
    std::cout << "Destination arrival time: " << route.arrival_time << " s" << std::endl;
    std::cout << "Peak AoI (arrival - gen): " << route.peak_aoi << " s" << std::endl;
    std::cout << "Replication quota K_b*: " << route.replication_quota << std::endl;

    // Check optimality: the 3-hop path arrives at ~80.04s, Peak AoI ~75.04s, well within 100s deadline
    assert(route.hops.size() == 3);
    assert(route.peak_aoi < 100.0);
    assert(route.replication_quota >= 1 && route.replication_quota <= 3);

    // Now test a tight deadline (D_b = 50s): the 3-hop arrives at 80s, so it must fail deadline check
    concgr::Bundle tight_bundle = bundle;
    tight_bundle.deadline_sec = 50.0; // Impossible to deliver by t_gen + 50 = 55s
    concgr::RoutePlan tight_route = router.compute_route(1, 0, tight_bundle, plan, 5.0);
    assert(!tight_route.found);
    std::cout << "[PASS] Deadline pruning successfully rejected impossible path." << std::endl;

    std::cout << "[PASS] Conformal Dijkstra correctly optimizes Peak-AoI store-carry-forward!" << std::endl;
    return 0;
}
