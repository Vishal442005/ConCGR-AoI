#include "concgr_core/knife_edge_diffraction.hpp"
#include <cmath>
#include <algorithm>

namespace concgr {

KnifeEdgeDiffractionModel::KnifeEdgeDiffractionModel()
    : KnifeEdgeDiffractionModel(Config{})
{
}

KnifeEdgeDiffractionModel::KnifeEdgeDiffractionModel(const Config& config)
    : config_(config), wavelength_(299792458.0 / config.frequency_hz)
{
}

double KnifeEdgeDiffractionModel::map_snr_to_bitrate(double snr_db) const {
    // Standard 802.11p 10 MHz channel rates:
    if (snr_db < 5.0) return 0.0;
    if (snr_db < 6.0) return 3.0e6;
    if (snr_db < 8.0) return 4.5e6;
    if (snr_db < 11.0) return 6.0e6;
    if (snr_db < 15.0) return 9.0e6;
    if (snr_db < 18.0) return 12.0e6;
    if (snr_db < 22.0) return 18.0e6;
    if (snr_db < 24.0) return 24.0e6;
    return 27.0e6;
}

DiffractionResult KnifeEdgeDiffractionModel::evaluate_link(
    const Point3D& tx, const Point3D& rx, const TerrainGrid& terrain) const
{
    DiffractionResult res;
    double dx = rx.x - tx.x;
    double dy = rx.y - tx.y;
    double dz = rx.z - tx.z;
    double dist_2d = std::sqrt(dx * dx + dy * dy);
    double dist_3d = std::sqrt(dist_2d * dist_2d + dz * dz);
    res.distance_3d_m = dist_3d;

    // Check maximum radio range limit
    if (dist_3d > config_.max_comm_range_m || dist_3d < 1.0) {
        res.has_los = false;
        res.is_link_active = false;
        res.rx_power_dbm = -150.0;
        res.snr_db = -50.0;
        return res;
    }

    // 1. Free Space Path Loss (Friis in dB)
    double fspl = 20.0 * std::log10(dist_3d) + 20.0 * std::log10(config_.frequency_hz) - 147.55;
    res.free_space_loss_db = fspl;

    // 2. Sample 3D terrain profile along the ray
    double step_m = terrain.resolution_m;
    size_t num_samples = static_cast<size_t>(std::max(2.0, std::floor(dist_2d / step_m)));

    double max_nu = -999.0;
    bool obstructed = false;

    for (size_t i = 1; i < num_samples; ++i) {
        double frac = static_cast<double>(i) / static_cast<double>(num_samples);
        double sample_x = tx.x + frac * dx;
        double sample_y = tx.y + frac * dy;
        double los_z = tx.z + frac * dz;

        double d1 = frac * dist_3d;
        double d2 = (1.0 - frac) * dist_3d;

        float terr_z = terrain.get_elevation(sample_x, sample_y);

        // Clearance h: positive if terrain penetrates above LoS ray
        double h = static_cast<double>(terr_z) - los_z;

        if (h > 0.0) {
            obstructed = true;
        }

        // Fresnel-Kirchhoff parameter: nu = h * sqrt(2*(d1+d2) / (lambda * d1 * d2))
        double nu = h * std::sqrt((2.0 * dist_3d) / (wavelength_ * d1 * d2));
        if (nu > max_nu) {
            max_nu = nu;
        }
    }

    res.max_nu = max_nu;
    res.has_los = !obstructed;

    // 3. Knife-Edge Diffraction Loss J(nu) via ITU-R P.526-15
    double j_nu = 0.0;
    if (max_nu > -0.7) {
        double term = std::sqrt((max_nu - 0.1) * (max_nu - 0.1) + 1.0) + max_nu - 0.1;
        if (term > 0.0) {
            j_nu = 6.9 + 20.0 * std::log10(term);
        }
    }
    j_nu = std::max(0.0, j_nu);
    res.knife_edge_loss_db = j_nu;

    // 4. Total Path Loss & Received Signal Power
    res.total_path_loss_db = fspl + j_nu;
    res.rx_power_dbm = config_.tx_power_dbm + config_.tx_gain_dbi + config_.rx_gain_dbi - res.total_path_loss_db;
    res.snr_db = res.rx_power_dbm - config_.noise_floor_dbm;

    // 5. Link Active & Rate Adaptation
    res.is_link_active = (res.rx_power_dbm >= config_.rx_sensitivity_dbm) && (res.snr_db >= 5.0);
    if (res.is_link_active) {
        res.effective_rate_bps = map_snr_to_bitrate(res.snr_db);
    } else {
        res.effective_rate_bps = 0.0;
    }

    return res;
}

} // namespace concgr
