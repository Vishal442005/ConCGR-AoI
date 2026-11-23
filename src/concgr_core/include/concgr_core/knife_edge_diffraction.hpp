#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

namespace concgr {

struct Point3D {
    double x{0.0}; // Easting / local x (m)
    double y{0.0}; // Northing / local y (m)
    double z{0.0}; // Altitude ASL (m)
};

struct TerrainGrid {
    double origin_x{0.0};
    double origin_y{0.0};
    double resolution_m{30.87};
    size_t rows{0};
    size_t cols{0};
    std::vector<float> elevation_data; // row-major, size = rows * cols

    float get_elevation(double x, double y) const {
        if (rows == 0 || cols == 0 || elevation_data.empty()) return 0.0f;
        double col_d = (x - origin_x) / resolution_m;
        double row_d = (y - origin_y) / resolution_m;

        int c = std::clamp(static_cast<int>(col_d), 0, static_cast<int>(cols - 1));
        int r = std::clamp(static_cast<int>(row_d), 0, static_cast<int>(rows - 1));
        return elevation_data[r * cols + c];
    }
};

struct DiffractionResult {
    bool has_los{true};
    double distance_3d_m{0.0};
    double free_space_loss_db{0.0};
    double knife_edge_loss_db{0.0};
    double total_path_loss_db{0.0};
    double rx_power_dbm{0.0};
    double snr_db{0.0};
    double max_nu{-999.0};
    double effective_rate_bps{0.0};
    bool is_link_active{false};
};

class KnifeEdgeDiffractionModel {
public:
    struct Config {
        double frequency_hz{5.9e9};       // 5.9 GHz (IEEE 802.11p DSRC / ITS band)
        double tx_power_dbm{23.0};        // 200 mW EIRP
        double tx_gain_dbi{2.0};
        double rx_gain_dbi{2.0};
        double noise_floor_dbm{-95.0};
        double rx_sensitivity_dbm{-85.0}; // Minimum sensitivity for 802.11p 6 Mbps BPSK
        double max_comm_range_m{1500.0};  // Nominal radio horizon limit
    };

    KnifeEdgeDiffractionModel();
    explicit KnifeEdgeDiffractionModel(const Config& config);

    // Evaluate ITU-R P.526 knife-edge diffraction and received power along 3D ray
    DiffractionResult evaluate_link(const Point3D& tx, const Point3D& rx, const TerrainGrid& terrain) const;

    // Rate adaptation lookup based on SNR (802.11p MCS 0 to MCS 7)
    double map_snr_to_bitrate(double snr_db) const;

private:
    Config config_;
    double wavelength_{0.0508}; // 5.9 GHz ~ 5.08 cm
};

} // namespace concgr
