#pragma once

#include "concgr_core/types.hpp"
#include "concgr_core/replication_engine.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>

namespace concgr {

class ConformalDijkstraRouter {
public:
    struct Config {
        uint32_t max_hops{6};
        double alpha_path{0.05};
        uint32_t k_max{3};
    };

    ConformalDijkstraRouter();
    explicit ConformalDijkstraRouter(const Config& config);

    // Compute minimax Peak-AoI route over admitted conformal contacts
    RoutePlan compute_route(
        uint32_t src_id,
        uint32_t dst_id,
        const Bundle& bundle,
        const std::vector<ConformalContact>& contact_plan,
        double current_time
    ) const;

private:
    Config config_;
    ReplicationEngine rep_engine_;
};

} // namespace concgr
