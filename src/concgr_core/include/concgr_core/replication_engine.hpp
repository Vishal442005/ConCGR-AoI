#pragma once

#include "concgr_core/types.hpp"
#include <cmath>
#include <algorithm>

namespace concgr {

class ReplicationEngine {
public:
    struct Config {
        uint32_t k_max{3};
        double alpha_path_target{0.05};
    };

    ReplicationEngine();
    explicit ReplicationEngine(const Config& config);

    // Calculate risk-calibrated replication quota K_b* (Eq. 11)
    uint32_t compute_replication_quota(size_t hop_count, double alpha_link_per_hop) const;

private:
    Config config_;
};

inline ReplicationEngine::ReplicationEngine()
    : ReplicationEngine(Config{})
{
}

inline ReplicationEngine::ReplicationEngine(const Config& config)
    : config_(config)
{
}

inline uint32_t ReplicationEngine::compute_replication_quota(size_t hop_count, double alpha_link_per_hop) const {
    if (hop_count == 0) {
        return 1;
    }

    double p_hop_safe = std::clamp(1.0 - alpha_link_per_hop, 0.01, 0.999);
    double p_path_safe = std::pow(p_hop_safe, static_cast<double>(hop_count));
    double p_path_fail = std::clamp(1.0 - p_path_safe, 1e-6, 0.999);

    double k_ideal = std::log(config_.alpha_path_target) / std::log(p_path_fail);
    if (std::isnan(k_ideal) || k_ideal < 1.0) {
        return 1;
    }

    uint32_t k_star = static_cast<uint32_t>(std::ceil(k_ideal));
    return std::clamp(k_star, 1u, config_.k_max);
}

} // namespace concgr
