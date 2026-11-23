#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>

namespace concgr {

struct ContactNominal {
    uint32_t id{0};
    uint32_t tx_id{0};
    uint32_t rx_id{0};
    double t_start{0.0};      // seconds
    double t_end{0.0};        // seconds
    double rate_bps{0.0};     // bits per second
};

struct ContactRealized {
    uint32_t id{0};
    uint32_t tx_id{0};
    uint32_t rx_id{0};
    double t_start_real{0.0};
    double t_end_real{0.0};
    double rate_bps_real{0.0};
};

struct ContactResiduals {
    double e_s{0.0};  // max(0, t_start_real - t_start) [start lag]
    double e_e{0.0};  // max(0, t_end - t_end_real)     [early cutoff]
    double e_r{0.0};  // max(0, 1.0 - rate_real / rate) [throughput degradation]
};

struct ConformalContact {
    uint32_t id{0};
    uint32_t tx_id{0};
    uint32_t rx_id{0};
    double t_start_conf{0.0};     // t_start + q_s
    double t_end_conf{0.0};       // t_end - q_e
    double effective_rate_bps{0.0}; // rate * (1 - q_r)
    double effective_volume_bits{0.0};
    double booked_volume_bits{0.0};
    bool is_admitted{false};
};

struct Bundle {
    uint64_t id{0};
    uint32_t src{0};
    uint32_t dst{0};
    uint32_t priority{1};       // 1 = Emergency (30s), 2 = Map (90s), 3 = Telemetry (240s)
    double t_gen{0.0};          // Generation timestamp (s)
    double deadline_sec{60.0};  // D_b (s)
    uint32_t size_bytes{1024};  // S_b (bytes)
    std::vector<uint8_t> payload;
    uint32_t replication_quota{1}; // K_b*
    uint32_t copy_index{0};
    std::vector<uint32_t> hop_history;
};

struct RouteHop {
    uint32_t contact_id{0};
    uint32_t tx_id{0};
    uint32_t rx_id{0};
    double t_tx_start{0.0};
    double t_tx_end{0.0};
    double rate_bps{0.0};
};

struct RoutePlan {
    bool found{false};
    std::vector<RouteHop> hops;
    double arrival_time{0.0};
    double peak_aoi{0.0};       // arrival_time - t_gen
    uint32_t replication_quota{1};
};

} // namespace concgr
