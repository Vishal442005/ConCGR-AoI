#include "concgr_core/swarm_simulator.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>

struct MetricStats {
    double mean{0.0};
    double std{0.0};
    double min{0.0};
    double max{0.0};
};

MetricStats compute_stats(const std::vector<double>& vals) {
    MetricStats s;
    if (vals.empty()) return s;
    double sum = std::accumulate(vals.begin(), vals.end(), 0.0);
    s.mean = sum / vals.size();

    double sq_sum = 0.0;
    for (double v : vals) sq_sum += (v - s.mean) * (v - s.mean);
    s.std = (vals.size() > 1) ? std::sqrt(sq_sum / (vals.size() - 1)) : 0.0;

    s.min = *std::min_element(vals.begin(), vals.end());
    s.max = *std::max_element(vals.begin(), vals.end());
    return s;
}

int main() {
    std::cout << "================================================================================" << std::endl;
    std::cout << "ConCGR-AoI: Full-Scale Empirical Benchmark Campaign (Phase 2 Execution)" << std::endl;
    std::cout << "Terrain: Real NASA SRTM 30m DEM (Kangra Valley, Himalayas: 5 km x 5 km)" << std::endl;
    std::cout << "Mobility: 8 Autonomous Patrol UAVs + 1 Ground Station Sink (Node 0)" << std::endl;
    std::cout << "Disturbance: Real Open-Meteo Wind Field (6.5 m/s Northeast shift at t = 600s)" << std::endl;
    std::cout << "Channel Model: ITU-R P.526 Knife-Edge Diffraction + 802.11p PHY rate fallback" << std::endl;
    std::cout << "================================================================================" << std::endl;

    std::vector<std::string> protocols = {
        "Connected-FANET",
        "Epidemic",
        "Spray-and-Wait",
        "PRoPHETv2",
        "Deterministic-CGR",
        "ConCGR-AoI"
    };

    const size_t num_seeds = 5; // 5 full 20-minute mission seeds per protocol
    const std::vector<uint32_t> seeds = {101, 202, 303, 404, 505};

    concgr::SwarmSimulator::Config cfg;
    cfg.num_uavs = 8;
    cfg.mission_duration_sec = 1200.0; // 20 minutes
    cfg.time_step_sec = 1.0;
    cfg.wind_shift_time_sec = 600.0;
    cfg.dem_path = "/home/jarvis/concgr_dtn_ws/data/terrain/kangra_valley_real_srtm30m.csv";

    concgr::SwarmSimulator sim(cfg);

    // Storage for results: protocol -> list of SimMetrics
    std::unordered_map<std::string, std::vector<concgr::SimMetrics>> results;

    std::ofstream raw_csv("/home/jarvis/concgr_dtn_ws/data/benchmark_results_raw.csv");
    raw_csv << "Protocol,Seed,TotalGen,Delivered,BDR_Pct,DMR_Pct,P50_AoI_s,P95_AoI_s,P99_AoI_s,Overhead,Evictions,SolveTime_us\n";

    for (uint32_t seed : seeds) {
        std::cout << "\n>>> Initializing Real Scenario Seed: " << seed << " ... ";
        sim.setup_scenario(seed);
        std::cout << "Done! (Extracted " << sim.get_nominal_plan().size()
                  << " nominal contacts, " << sim.get_realized_plan().size() << " realized contacts)" << std::endl;

        for (const auto& proto : protocols) {
            std::cout << "    Running " << std::left << std::setw(20) << proto << " ... " << std::flush;
            auto m = sim.run_protocol(proto, seed);
            results[proto].push_back(m);

            std::cout << "BDR: " << std::fixed << std::setprecision(1) << m.bdr << "% | "
                      << "P95 AoI: " << std::setprecision(1) << m.p95_paoi_sec << "s | "
                      << "Overhead: " << std::setprecision(2) << m.overhead_ratio << "x | "
                      << "Evictions: " << m.total_evicted << " | "
                      << "Solve: " << std::setprecision(1) << m.avg_solve_time_us << " us" << std::endl;

            raw_csv << m.protocol_name << "," << m.seed << "," << m.total_generated << ","
                    << m.total_delivered << "," << m.bdr << "," << m.dmr << ","
                    << m.p50_paoi_sec << "," << m.p95_paoi_sec << "," << m.p99_paoi_sec << ","
                    << m.overhead_ratio << "," << m.total_evicted << "," << m.avg_solve_time_us << "\n";
            raw_csv.flush();
        }
    }
    raw_csv.close();

    // Generate and display Publication-Grade Summary Table
    std::cout << "\n\n==========================================================================================================" << std::endl;
    std::cout << "TABLE 2: TIER-A REAL-DATA BENCHMARK RESULTS (KANGRA VALLEY DEM, 5 SEEDS x 20 MIN MISSIONS)" << std::endl;
    std::cout << "==========================================================================================================" << std::endl;
    std::cout << std::left << std::setw(22) << "Protocol"
              << std::setw(18) << "BDR (%)"
              << std::setw(18) << "P95 AoI (s)"
              << std::setw(18) << "DMR (%)"
              << std::setw(18) << "Overhead Ratio"
              << std::setw(16) << "Evictions"
              << "Solve Time" << std::endl;
    std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;

    std::ofstream pub_csv("/home/jarvis/concgr_dtn_ws/data/publication_table_tier_a.csv");
    pub_csv << "Protocol,BDR_Mean,BDR_Std,P95_AoI_Mean,P95_AoI_Std,DMR_Mean,DMR_Std,Overhead_Mean,Overhead_Std,Evictions_Mean,SolveTime_us\n";

    for (const auto& proto : protocols) {
        const auto& m_list = results[proto];
        std::vector<double> bdrs, p95s, dmrs, ovhs, evicts, solves;
        for (const auto& m : m_list) {
            bdrs.push_back(m.bdr);
            p95s.push_back(m.p95_paoi_sec);
            dmrs.push_back(m.dmr);
            ovhs.push_back(m.overhead_ratio);
            evicts.push_back(static_cast<double>(m.total_evicted));
            solves.push_back(m.avg_solve_time_us);
        }

        auto s_bdr = compute_stats(bdrs);
        auto s_p95 = compute_stats(p95s);
        auto s_dmr = compute_stats(dmrs);
        auto s_ovh = compute_stats(ovhs);
        auto s_evi = compute_stats(evicts);
        auto s_sol = compute_stats(solves);

        std::stringstream ss_bdr, ss_p95, ss_dmr, ss_ovh, ss_evi, ss_sol;
        ss_bdr << std::fixed << std::setprecision(1) << s_bdr.mean << " +/- " << s_bdr.std;
        ss_p95 << std::fixed << std::setprecision(1) << s_p95.mean << " +/- " << s_p95.std;
        ss_dmr << std::fixed << std::setprecision(1) << s_dmr.mean << " +/- " << s_dmr.std;
        ss_ovh << std::fixed << std::setprecision(2) << s_ovh.mean << " +/- " << s_ovh.std;
        ss_evi << std::fixed << std::setprecision(0) << s_evi.mean;
        ss_sol << std::fixed << std::setprecision(1) << s_sol.mean << " us";

        std::cout << std::left << std::setw(22) << proto
                  << std::setw(18) << ss_bdr.str()
                  << std::setw(18) << ss_p95.str()
                  << std::setw(18) << ss_dmr.str()
                  << std::setw(18) << ss_ovh.str()
                  << std::setw(16) << ss_evi.str()
                  << ss_sol.str() << std::endl;

        pub_csv << proto << ","
                << s_bdr.mean << "," << s_bdr.std << ","
                << s_p95.mean << "," << s_p95.std << ","
                << s_dmr.mean << "," << s_dmr.std << ","
                << s_ovh.mean << "," << s_ovh.std << ","
                << s_evi.mean << "," << s_sol.mean << "\n";
    }
    pub_csv.close();

    std::cout << "==========================================================================================================" << std::endl;
    std::cout << "Benchmark results exported to:" << std::endl;
    std::cout << "  1. Raw Seed Traces:       /home/jarvis/concgr_dtn_ws/data/benchmark_results_raw.csv" << std::endl;
    std::cout << "  2. Publication Summary:   /home/jarvis/concgr_dtn_ws/data/publication_table_tier_a.csv" << std::endl;
    std::cout << "==========================================================================================================" << std::endl;

    return 0;
}
