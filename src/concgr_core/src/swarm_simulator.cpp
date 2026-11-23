#include "concgr_core/swarm_simulator.hpp"
#include "concgr_core/epidemic_router.hpp"
#include "concgr_core/spray_and_wait_router.hpp"
#include "concgr_core/prophet_router.hpp"
#include "concgr_core/deterministic_cgr_router.hpp"
#include "concgr_core/connected_fanet_router.hpp"
#include "concgr_core/concgr_aoi_router.hpp"

#include <fstream>
#include <sstream>
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace concgr {

SwarmSimulator::SwarmSimulator()
    : SwarmSimulator(Config{})
{
}

SwarmSimulator::SwarmSimulator(const Config& config)
    : config_(config),
      terrain_(load_terrain_dem(config.dem_path))
{
}

TerrainGrid SwarmSimulator::load_terrain_dem(const std::string& csv_path) const {
    TerrainGrid grid;
    grid.origin_x = 0.0;
    grid.origin_y = 0.0;
    grid.resolution_m = 30.87;

    std::ifstream file(csv_path);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open DEM file: " << csv_path << std::endl;
        return grid;
    }

    std::string line;
    std::vector<std::vector<float>> rows;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string val;
        std::vector<float> row;
        while (std::getline(ss, val, ',')) {
            row.push_back(std::stof(val));
        }
        rows.push_back(row);
    }

    grid.rows = rows.size();
    grid.cols = rows.empty() ? 0 : rows[0].size();
    grid.elevation_data.reserve(grid.rows * grid.cols);
    for (const auto& r : rows) {
        for (float v : r) {
            grid.elevation_data.push_back(v);
        }
    }

    return grid;
}

void SwarmSimulator::generate_trajectories(uint32_t seed) {
    std::mt19937 rng(seed);
    size_t total_steps = static_cast<size_t>(config_.mission_duration_sec / config_.time_step_sec) + 1;

    trajectories_nominal_.assign(config_.num_uavs + 1, std::vector<Point3D>(total_steps));
    trajectories_realized_.assign(config_.num_uavs + 1, std::vector<Point3D>(total_steps));

    // Node 0: Fixed Ground Station Sink in south valley
    double sink_x = 2500.0;
    double sink_y = 500.0;
    double sink_z = terrain_.get_elevation(sink_x, sink_y) + 10.0; // 10m antenna tower
    for (size_t s = 0; s < total_steps; ++s) {
        trajectories_nominal_[0][s] = {sink_x, sink_y, sink_z};
        trajectories_realized_[0][s] = {sink_x, sink_y, sink_z};
    }

    // UAVs 1 to num_uavs: Patrol loops across ridges and valleys
    const double cruise_speed = 15.0; // 15 m/s (~54 km/h)
    std::uniform_real_distribution<double> phase_dist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radius_x_dist(800.0, 1600.0);
    std::uniform_real_distribution<double> radius_y_dist(600.0, 1200.0);
    std::uniform_real_distribution<double> center_dist(1500.0, 3500.0);

    for (uint32_t u = 1; u <= config_.num_uavs; ++u) {
        double cx = center_dist(rng);
        double cy = center_dist(rng);
        double rx = radius_x_dist(rng);
        double ry = radius_y_dist(rng);
        double phase = phase_dist(rng);
        double period = (2.0 * M_PI * std::max(rx, ry)) / cruise_speed;

        double wind_delay_accum = 0.0;

        for (size_t s = 0; s < total_steps; ++s) {
            double t = s * config_.time_step_sec;

            // Nominal circular / elliptical patrol trajectory
            double angle_nom = phase + (2.0 * M_PI * t) / period;
            double nom_x = cx + rx * std::cos(angle_nom);
            double nom_y = cy + ry * std::sin(angle_nom);
            double ground_z_nom = terrain_.get_elevation(nom_x, nom_y);
            double nom_z = ground_z_nom + 150.0; // 150m AGL

            trajectories_nominal_[u][s] = {nom_x, nom_y, nom_z};

            // Realized trajectory with real wind-induced drift
            // At t >= 600s, real Kangra wind kicks in (headwind drag)
            if (t >= config_.wind_shift_time_sec) {
                // Wind vector from NE (45 deg) at 6.5 m/s with gusts
                double wind_speed = 6.5 + 2.0 * std::sin(t * 0.05 + u);
                // Flight heading angle
                double heading = angle_nom + M_PI_2;
                double headwind = wind_speed * std::cos(heading - (M_PI * 0.25));
                // Speed reduction
                double speed_loss = std::max(0.0, headwind * 0.8);
                wind_delay_accum += (speed_loss / cruise_speed) * config_.time_step_sec;
            }

            double t_eff = std::max(0.0, t - wind_delay_accum);
            double angle_real = phase + (2.0 * M_PI * t_eff) / period;
            double real_x = cx + rx * std::cos(angle_real);
            double real_y = cy + ry * std::sin(angle_real);
            double ground_z_real = terrain_.get_elevation(real_x, real_y);
            double real_z = ground_z_real + 150.0;

            trajectories_realized_[u][s] = {real_x, real_y, real_z};
        }
    }
}

void SwarmSimulator::compute_contact_plans() {
    nominal_plan_.clear();
    realized_plan_.clear();

    size_t total_steps = static_cast<size_t>(config_.mission_duration_sec / config_.time_step_sec) + 1;

    // Helper lambda to detect contact intervals from step-by-step links
    auto extract_contacts = [this, total_steps](
        const std::vector<std::vector<Point3D>>& trajs,
        bool is_realized
    ) -> std::vector<ContactNominal> {
        (void)is_realized;
        std::vector<ContactNominal> contacts;
        uint32_t cid = 1;

        for (uint32_t i = 0; i <= config_.num_uavs; ++i) {
            for (uint32_t j = 0; j <= config_.num_uavs; ++j) {
                if (i == j) continue;

                bool in_contact = false;
                double c_start = 0.0;
                double rate_sum = 0.0;
                size_t rate_count = 0;

                for (size_t s = 0; s < total_steps; ++s) {
                    double t = s * config_.time_step_sec;
                    const auto& p1 = trajs[i][s];
                    const auto& p2 = trajs[j][s];

                    auto link = diffraction_model_.evaluate_link(p1, p2, terrain_);

                    if (link.is_link_active) {
                        if (!in_contact) {
                            in_contact = true;
                            c_start = t;
                            rate_sum = 0.0;
                            rate_count = 0;
                        }
                        rate_sum += link.effective_rate_bps;
                        rate_count++;
                    } else {
                        if (in_contact) {
                            in_contact = false;
                            double c_end = t;
                            if (c_end - c_start >= 3.0) { // Minimum 3s contact duration
                                ContactNominal c;
                                c.id = cid++;
                                c.tx_id = i;
                                c.rx_id = j;
                                c.t_start = c_start;
                                c.t_end = c_end;
                                c.rate_bps = (rate_count > 0) ? (rate_sum / rate_count) : 6.0e6;
                                contacts.push_back(c);
                            }
                        }
                    }
                }
            }
        }
        return contacts;
    };

    nominal_plan_ = extract_contacts(trajectories_nominal_, false);
    auto raw_real = extract_contacts(trajectories_realized_, true);

    // Map raw realized contacts to nominal IDs
    realized_plan_.clear();
    for (const auto& nom : nominal_plan_) {
        // Find best matching realized contact
        ContactRealized match;
        match.id = nom.id;
        match.tx_id = nom.tx_id;
        match.rx_id = nom.rx_id;
        match.t_start_real = nom.t_start;
        match.t_end_real = nom.t_end;
        match.rate_bps_real = nom.rate_bps;

        double best_overlap = -1.0;
        for (const auto& r : raw_real) {
            if (r.tx_id == nom.tx_id && r.rx_id == nom.rx_id) {
                double overlap_start = std::max(nom.t_start, r.t_start);
                double overlap_end = std::min(nom.t_end, r.t_end);
                double overlap = overlap_end - overlap_start;
                if (overlap > best_overlap) {
                    best_overlap = overlap;
                    match.t_start_real = r.t_start;
                    match.t_end_real = r.t_end;
                    match.rate_bps_real = r.rate_bps;
                }
            }
        }
        realized_plan_.push_back(match);
    }
}

void SwarmSimulator::generate_sensor_traffic(uint32_t seed) {
    generated_bundles_.clear();
    std::mt19937 rng(seed + 100);

    uint64_t bundle_id_counter = 1;

    // Traffic generation intervals:
    // Every 45s: Thermal Wildfire Alert (Priority 1, D_b = 30s, 25 KB)
    // Every 90s: LiDAR Sub-map (Priority 2, D_b = 90s, 180 KB)
    // Every 30s: Flight Telemetry (Priority 3, D_b = 240s, 5 KB)

    for (double t = 15.0; t < config_.mission_duration_sec - 60.0; t += 15.0) {
        for (uint32_t u = 1; u <= config_.num_uavs; ++u) {
            std::uniform_real_distribution<double> jitter(0.0, 5.0);
            double gen_time = t + jitter(rng);

            if (static_cast<int>(t) % 45 == 0) {
                // Emergency Thermal Alert
                Bundle b;
                b.id = bundle_id_counter++;
                b.src = u;
                b.dst = 0; // Ground Control Station Sink
                b.priority = 3; // Highest eviction protection
                b.t_gen = gen_time;
                b.deadline_sec = 30.0;
                b.size_bytes = 25 * 1024; // 25 KB
                generated_bundles_.push_back({gen_time, b});
            } else if (static_cast<int>(t) % 90 == 0) {
                // LiDAR Sub-map
                Bundle b;
                b.id = bundle_id_counter++;
                b.src = u;
                b.dst = 0;
                b.priority = 2;
                b.t_gen = gen_time;
                b.deadline_sec = 90.0;
                b.size_bytes = 180 * 1024; // 180 KB
                generated_bundles_.push_back({gen_time, b});
            } else if (static_cast<int>(t) % 30 == 0) {
                // Routine Flight Telemetry
                Bundle b;
                b.id = bundle_id_counter++;
                b.src = u;
                b.dst = 0;
                b.priority = 1;
                b.t_gen = gen_time;
                b.deadline_sec = 240.0;
                b.size_bytes = 5 * 1024; // 5 KB
                generated_bundles_.push_back({gen_time, b});
            }
        }
    }

    std::sort(generated_bundles_.begin(), generated_bundles_.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
}

void SwarmSimulator::setup_scenario(uint32_t seed) {
    generate_trajectories(seed);
    compute_contact_plans();
    generate_sensor_traffic(seed);
}

SimMetrics SwarmSimulator::run_protocol(const std::string& protocol_name, uint32_t seed) {
    SimMetrics m;
    m.protocol_name = protocol_name;
    m.seed = seed;
    m.total_generated = generated_bundles_.size();

    // Create a router instance per node (nodes 0 to num_uavs)
    std::vector<std::unique_ptr<DtnRouterBase>> routers;
    routers.resize(config_.num_uavs + 1);

    for (uint32_t i = 0; i <= config_.num_uavs; ++i) {
        if (protocol_name == "Epidemic") {
            routers[i] = std::make_unique<EpidemicRouter>(i);
        } else if (protocol_name == "Spray-and-Wait") {
            routers[i] = std::make_unique<SprayAndWaitRouter>(i);
        } else if (protocol_name == "PRoPHETv2") {
            routers[i] = std::make_unique<ProphetRouter>(i);
        } else if (protocol_name == "Deterministic-CGR") {
            routers[i] = std::make_unique<DeterministicCgrRouter>(i, nominal_plan_);
        } else if (protocol_name == "Connected-FANET") {
            routers[i] = std::make_unique<ConnectedFanetRouter>(i);
        } else { // "ConCGR-AoI"
            routers[i] = std::make_unique<ConCgrAoiRouter>(i, nominal_plan_);
        }
    }

    // Tracking delivered bundles at sink node 0
    std::unordered_map<uint64_t, double> delivery_times;
    std::vector<double> peak_aois;

    // Active realized contacts tracking: contact_id -> is_open
    std::vector<bool> contact_open(realized_plan_.size(), false);

    size_t traffic_idx = 0;
    size_t total_fwd = 0;
    size_t total_solve_calls = 0;
    double total_solve_time_us = 0.0;

    // Simulation loop (discrete seconds)
    for (double t = 0.0; t <= config_.mission_duration_sec; t += config_.time_step_sec) {
        // 1. Inject newly generated sensor traffic
        while (traffic_idx < generated_bundles_.size() && generated_bundles_[traffic_idx].first <= t) {
            const auto& b = generated_bundles_[traffic_idx].second;
            routers[b.src]->on_bundle_generated(b, t);
            traffic_idx++;
        }

        // 2. Open/Close REALIZED contact windows at current time
        for (size_t c_idx = 0; c_idx < realized_plan_.size(); ++c_idx) {
            const auto& r = realized_plan_[c_idx];
            bool currently_in_range = (t >= r.t_start_real && t < r.t_end_real);

            if (currently_in_range && !contact_open[c_idx]) {
                contact_open[c_idx] = true;
                routers[r.tx_id]->on_contact_start(r.id, r.rx_id, t, r.rate_bps_real);
            } else if (!currently_in_range && contact_open[c_idx]) {
                contact_open[c_idx] = false;
                routers[r.tx_id]->on_contact_end(r.id, r.rx_id, t);

                // For ConCGR-AoI, deliver contact feedback upon completion
                if (protocol_name == "ConCGR-AoI") {
                    auto* c_router = dynamic_cast<ConCgrAoiRouter*>(routers[r.tx_id].get());
                    if (c_router && c_idx < nominal_plan_.size()) {
                        c_router->on_contact_feedback(nominal_plan_[c_idx], r);
                    }
                }
            }
        }

        // 3. Execute router forward decisions for all nodes
        for (uint32_t i = 1; i <= config_.num_uavs; ++i) {
            auto t_start = std::chrono::high_resolution_clock::now();
            auto decisions = routers[i]->step(t);
            auto t_end = std::chrono::high_resolution_clock::now();

            double solve_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
            total_solve_time_us += solve_us;
            total_solve_calls++;

            for (const auto& dec : decisions) {
                total_fwd++;
                // Find bundle in router i queue
                const auto& q = routers[i]->get_queue();
                auto b_it = std::find_if(q.begin(), q.end(),
                    [&dec](const Bundle& b) { return b.id == dec.bundle_id; });

                if (b_it != q.end()) {
                    Bundle transmitted_bundle = *b_it;
                    // Deliver to next hop
                    uint32_t nxt = dec.next_hop_uav_id;
                    routers[nxt]->on_bundle_received(transmitted_bundle, t);

                    // If reached Sink Node 0: Delivery event!
                    if (nxt == 0) {
                        if (delivery_times.find(transmitted_bundle.id) == delivery_times.end()) {
                            delivery_times[transmitted_bundle.id] = t;
                            double paoi = t - transmitted_bundle.t_gen;
                            peak_aois.push_back(paoi);

                            // Broadcast custody acknowledgment to clear copies
                            for (uint32_t u = 0; u <= config_.num_uavs; ++u) {
                                routers[u]->on_custody_ack(transmitted_bundle.id);
                            }
                        }
                    }
                }
            }
        }
    }

    // Compute final publication statistics
    m.total_delivered = delivery_times.size();
    m.total_forwardings = total_fwd;

    size_t deadline_met = 0;
    size_t deadline_missed = 0;
    for (const auto& pair : generated_bundles_) {
        const auto& b = pair.second;
        auto d_it = delivery_times.find(b.id);
        if (d_it != delivery_times.end()) {
            double latency = d_it->second - b.t_gen;
            if (latency <= b.deadline_sec) {
                deadline_met++;
            } else {
                deadline_missed++;
            }
        } else {
            deadline_missed++; // Never delivered = deadline missed
        }
    }

    m.total_deadline_met = deadline_met;
    m.total_deadline_missed = deadline_missed;

    m.bdr = (m.total_generated > 0) ? (static_cast<double>(m.total_delivered) / m.total_generated * 100.0) : 0.0;
    m.dmr = (m.total_generated > 0) ? (static_cast<double>(m.total_deadline_missed) / m.total_generated * 100.0) : 0.0;

    size_t total_evict = 0;
    for (uint32_t u = 0; u <= config_.num_uavs; ++u) {
        total_evict += routers[u]->get_total_evictions();
    }
    m.total_evicted = total_evict;

    if (m.total_delivered > 0) {
        m.overhead_ratio = static_cast<double>(m.total_forwardings - m.total_delivered) / static_cast<double>(m.total_delivered);
    } else {
        m.overhead_ratio = 0.0;
    }

    if (!peak_aois.empty()) {
        std::sort(peak_aois.begin(), peak_aois.end());
        size_t p50_idx = static_cast<size_t>(peak_aois.size() * 0.50);
        size_t p95_idx = static_cast<size_t>(peak_aois.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(peak_aois.size() * 0.99);

        p95_idx = std::min(p95_idx, peak_aois.size() - 1);
        p99_idx = std::min(p99_idx, peak_aois.size() - 1);

        m.p50_paoi_sec = peak_aois[p50_idx];
        m.p95_paoi_sec = peak_aois[p95_idx];
        m.p99_paoi_sec = peak_aois[p99_idx];
    }

    m.avg_solve_time_us = (total_solve_calls > 0) ? (total_solve_time_us / total_solve_calls) : 0.0;

    return m;
}

} // namespace concgr
