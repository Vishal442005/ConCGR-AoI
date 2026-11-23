#include "concgr_core/knife_edge_diffraction.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

int main() {
    std::cout << "=== Running Test: Knife-Edge Diffraction with Real Kangra Valley DEM ===" << std::endl;

    std::string csv_path = "/home/jarvis/concgr_dtn_ws/data/terrain/kangra_valley_real_srtm30m.csv";
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << csv_path << std::endl;
        return 1;
    }

    concgr::TerrainGrid terrain;
    terrain.origin_x = 0.0;
    terrain.origin_y = 0.0;
    terrain.resolution_m = 30.87;

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
    terrain.rows = rows.size();
    terrain.cols = rows.empty() ? 0 : rows[0].size();
    terrain.elevation_data.reserve(terrain.rows * terrain.cols);
    for (const auto& r : rows) {
        for (float v : r) {
            terrain.elevation_data.push_back(v);
        }
    }

    std::cout << "Loaded real DEM: " << terrain.rows << " rows x " << terrain.cols << " cols." << std::endl;

    concgr::KnifeEdgeDiffractionModel model;

    // Test Case 1: Two UAVs flying at high altitude with clear Line-of-Sight
    concgr::Point3D tx_los{500.0, 500.0, 2500.0};
    concgr::Point3D rx_los{1200.0, 500.0, 2500.0};
    auto res_los = model.evaluate_link(tx_los, rx_los, terrain);

    std::cout << "\nTest 1 (High altitude LoS):" << std::endl;
    std::cout << "  3D Distance: " << res_los.distance_3d_m << " m" << std::endl;
    std::cout << "  Has LoS: " << (res_los.has_los ? "YES" : "NO") << std::endl;
    std::cout << "  Diffraction Loss: " << res_los.knife_edge_loss_db << " dB" << std::endl;
    std::cout << "  Rx Power: " << res_los.rx_power_dbm << " dBm" << std::endl;
    std::cout << "  Link Active: " << (res_los.is_link_active ? "YES" : "NO") << std::endl;
    std::cout << "  Bitrate: " << res_los.effective_rate_bps / 1e6 << " Mbps" << std::endl;

    assert(res_los.has_los);
    assert(res_los.is_link_active);
    assert(res_los.effective_rate_bps >= 6.0e6);

    // Test Case 2: Two UAVs on opposite sides of a real mountain ridge
    // Row 50 (y = 1543.5m):
    // Tx at x = 300m, z = 1600m (valley elevation ~1550m)
    // Rx at x = 950m, z = 1600m (valley elevation ~1570m)
    // Intervening ridge at x = 463m has elevation 1624m (obstructs line-of-sight!)
    concgr::Point3D tx_nlos{300.0, 1543.5, 1600.0};
    concgr::Point3D rx_nlos{950.0, 1543.5, 1600.0};
    auto res_nlos = model.evaluate_link(tx_nlos, rx_nlos, terrain);

    std::cout << "\nTest 2 (Across mountain ridge NLoS):" << std::endl;
    std::cout << "  3D Distance: " << res_nlos.distance_3d_m << " m" << std::endl;
    std::cout << "  Has LoS: " << (res_nlos.has_los ? "YES" : "NO") << std::endl;
    std::cout << "  Diffraction Parameter nu: " << res_nlos.max_nu << std::endl;
    std::cout << "  Knife-Edge Loss: " << res_nlos.knife_edge_loss_db << " dB" << std::endl;
    std::cout << "  Rx Power: " << res_nlos.rx_power_dbm << " dBm" << std::endl;
    std::cout << "  Link Active: " << (res_nlos.is_link_active ? "YES" : "NO") << std::endl;

    assert(!res_nlos.has_los);
    assert(res_nlos.knife_edge_loss_db > 10.0); // True mountain ridge diffraction attenuation
    std::cout << "[PASS] ITU-R P.526 knife-edge model correctly computed mountain ridge diffraction attenuation!" << std::endl;

    return 0;
}
