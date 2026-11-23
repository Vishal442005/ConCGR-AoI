#pragma once

#include "concgr_core/types.hpp"
#include "concgr_core/router_interface.hpp"
#include "concgr_core/knife_edge_diffraction.hpp"
#include <vector>
#include <memory>
#include <string>

namespace concgr {

struct SimMetrics {
    std::string protocol_name;
    uint32_t seed{0};
    size_t total_generated{0};
    size_t total_delivered{0};
    size_t total_deadline_met{0};
    size_t total_deadline_missed{0};
    size_t total_evicted{0};
    size_t total_forwardings{0};

    double bdr{0.0};            // Bundle Delivery Ratio (delivered / generated)
    double dmr{0.0};            // Deadline Miss Ratio (missed / generated)
    double p50_paoi_sec{0.0};   // Median Peak-AoI
    double p95_paoi_sec{0.0};   // 95th Percentile Peak-AoI
    double p99_paoi_sec{0.0};   // 99th Percentile Peak-AoI
    double overhead_ratio{0.0}; // (total_forwardings - delivered) / delivered
    double eviction_rate{0.0};  // evicted / total_stored
    double avg_solve_time_us{0.0};
};

class SwarmSimulator {
public:
    struct Config {
        uint32_t num_uavs{8};                 // 8 UAVs + 1 Ground Station Sink (Node 0)
        double mission_duration_sec{1200.0};   // 20 minute mission
        double time_step_sec{1.0};            // 1s discrete simulation step
        double wind_shift_time_sec{600.0};    // Wind regime shift at t = 600s (Scenario S2)
        std::string dem_path{"/home/jarvis/concgr_dtn_ws/data/terrain/kangra_valley_real_srtm30m.csv"};
        std::string wind_path{"/home/jarvis/concgr_dtn_ws/data/wind/real_kangra_wind_telemetry.csv"};
    };

    SwarmSimulator();
    explicit SwarmSimulator(const Config& config);

    // Generate nominal contacts and true realized contacts over real Kangra DEM & wind field
    void setup_scenario(uint32_t seed);

    // Run a specific router protocol over the generated real mission
    SimMetrics run_protocol(const std::string& protocol_name, uint32_t seed);

    const std::vector<ContactNominal>& get_nominal_plan() const { return nominal_plan_; }
    const std::vector<ContactRealized>& get_realized_plan() const { return realized_plan_; }
    const std::vector<std::vector<Point3D>>& get_trajectories_realized() const { return trajectories_realized_; }
    const std::vector<std::pair<double, Bundle>>& get_generated_bundles() const { return generated_bundles_; }

private:
    TerrainGrid load_terrain_dem(const std::string& csv_path) const;
    void generate_trajectories(uint32_t seed);
    void compute_contact_plans();
    void generate_sensor_traffic(uint32_t seed);

    Config config_;
    TerrainGrid terrain_;
    KnifeEdgeDiffractionModel diffraction_model_;

    // UAV 3D trajectories: [uav_id][time_step] -> Point3D
    std::vector<std::vector<Point3D>> trajectories_nominal_;
    std::vector<std::vector<Point3D>> trajectories_realized_;

    std::vector<ContactNominal> nominal_plan_;
    std::vector<ContactRealized> realized_plan_;

    // Traffic bundles: [generation_time] -> vector of Bundles
    std::vector<std::pair<double, Bundle>> generated_bundles_;
};

} // namespace concgr
