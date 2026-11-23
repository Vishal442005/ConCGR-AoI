#include "concgr_core/epidemic_router.hpp"
#include "concgr_core/spray_and_wait_router.hpp"
#include "concgr_core/prophet_router.hpp"
#include "concgr_core/deterministic_cgr_router.hpp"
#include "concgr_core/connected_fanet_router.hpp"
#include "concgr_core/concgr_aoi_router.hpp"
#include <iostream>
#include <cassert>
#include <memory>

int main() {
    std::cout << "=== Running Test: Head-to-Head Comparative Router Verification ===" << std::endl;

    // Contact plan for a 3-hop store-carry-forward mission:
    // UAV 1 (src) -> UAV 2 -> UAV 3 -> UAV 0 (sink)
    std::vector<concgr::ContactNominal> nom_plan = {
        {1, 1, 2, 10.0, 30.0, 6.0e6},
        {2, 2, 3, 40.0, 70.0, 6.0e6},
        {3, 3, 0, 80.0, 110.0, 6.0e6}
    };

    // Instantiate all 6 routers at UAV 1
    std::vector<std::unique_ptr<concgr::DtnRouterBase>> routers;
    routers.push_back(std::make_unique<concgr::ConnectedFanetRouter>(1));
    routers.push_back(std::make_unique<concgr::EpidemicRouter>(1));
    routers.push_back(std::make_unique<concgr::SprayAndWaitRouter>(1));
    routers.push_back(std::make_unique<concgr::ProphetRouter>(1));
    routers.push_back(std::make_unique<concgr::DeterministicCgrRouter>(1, nom_plan));
    routers.push_back(std::make_unique<concgr::ConCgrAoiRouter>(1, nom_plan));

    // Bundle generated at t = 5s
    concgr::Bundle b;
    b.id = 42;
    b.src = 1;
    b.dst = 0;
    b.priority = 1;
    b.t_gen = 5.0;
    b.deadline_sec = 120.0;
    b.size_bytes = 25 * 1024; // 25 KB

    for (auto& r : routers) {
        std::cout << "\nTesting Protocol: " << r->get_protocol_name() << std::endl;
        r->on_bundle_generated(b, 5.0);

        // At t = 8s, no contact is open yet.
        auto dec_8 = r->step(8.0);
        std::cout << "  t = 8s (partition): " << dec_8.size() << " forwardings" << std::endl;

        // At t = 10s, contact 1 (1 -> 2) opens!
        r->on_contact_start(1, 2, 10.0, 6.0e6);
        auto dec_10 = r->step(10.0);
        std::cout << "  t = 10s (contact 1 -> 2 open): " << dec_10.size() << " forwardings" << std::endl;

        if (r->get_protocol_name().find("Connected-FANET") != std::string::npos) {
            // Connected FANET cannot forward because destination node 0 is not directly connected
            assert(dec_10.empty());
            std::cout << "  [PASS] Connected FANET correctly held/dropped packet under multi-hop partition!" << std::endl;
        } else {
            // All DTN routers must make a forward decision to UAV 2
            assert(!dec_10.empty());
            assert(dec_10[0].next_hop_uav_id == 2);
            std::cout << "  [PASS] DTN Router correctly forwarded bundle across first contact hop!" << std::endl;
        }

        r->on_contact_end(1, 2, 30.0);
    }

    std::cout << "\n=======================================================" << std::endl;
    std::cout << "ALL 6 ROUTING PROTOCOLS VERIFIED CLEANLY!" << std::endl;
    std::cout << "=======================================================" << std::endl;

    return 0;
}
