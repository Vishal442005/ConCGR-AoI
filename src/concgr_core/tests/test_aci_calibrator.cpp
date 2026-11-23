#include "concgr_core/aci_calibrator.hpp"
#include <iostream>
#include <cassert>
#include <random>

int main() {
    std::cout << "=== Running Test: ACI Calibrator (Lemma 1 Verification) ===" << std::endl;

    concgr::AciCalibrator::Config cfg;
    cfg.window_size = 100;
    cfg.beta = 0.98;
    cfg.gamma_s = 0.05;
    cfg.gamma_e = 0.05;
    cfg.gamma_r = 0.05;
    cfg.alpha_target_total = 0.06; // target link risk = 6% (2% per residual)

    concgr::AciCalibrator calibrator(cfg);

    // Simulate 500 contacts with non-stationary drift
    // First 250: calm flight (mean lag 2s, cutoff 1s, rate degradation 5%)
    // Second 250: heavy wind drift (mean lag 12s, cutoff 8s, rate degradation 25%)
    std::mt19937 rng(42);
    std::exponential_distribution<double> exp_lag_calm(1.0 / 2.0);
    std::exponential_distribution<double> exp_lag_wind(1.0 / 12.0);

    size_t failures = 0;
    const size_t total_contacts = 500;

    for (size_t i = 0; i < total_contacts; ++i) {
        concgr::ContactNominal nom;
        nom.id = static_cast<uint32_t>(i + 1);
        nom.tx_id = 1;
        nom.rx_id = 2;
        nom.t_start = 100.0 + i * 50.0;
        nom.t_end = nom.t_start + 30.0;
        nom.rate_bps = 6.0e6; // 6 Mbps

        // Before observing realized contact, get conformal prediction
        concgr::ConformalContact conf = calibrator.calibrate_contact(nom);

        // Realized contact with non-stationary regime shift at step 250
        concgr::ContactRealized real;
        real.id = nom.id;
        real.tx_id = nom.tx_id;
        real.rx_id = nom.rx_id;

        double lag = (i < 250) ? exp_lag_calm(rng) : exp_lag_wind(rng);
        double cutoff = (i < 250) ? (lag * 0.5) : (lag * 0.7);
        double rate_factor = (i < 250) ? 0.95 : 0.75;

        real.t_start_real = nom.t_start + lag;
        real.t_end_real = std::max(real.t_start_real, nom.t_end - cutoff);
        real.rate_bps_real = nom.rate_bps * rate_factor;

        // Check if realized volume fell below predicted conformal volume
        double real_duration = std::max(0.0, real.t_end_real - real.t_start_real);
        double real_volume = real_duration * real.rate_bps_real;

        if (conf.is_admitted && real_volume < conf.effective_volume_bits) {
            failures++;
        }

        // Online ACI feedback update
        calibrator.record_contact_feedback(nom, real);
    }

    double failure_rate = static_cast<double>(failures) / static_cast<double>(total_contacts);
    std::cout << "Total contacts: " << total_contacts << std::endl;
    std::cout << "Target alpha_link: " << cfg.alpha_target_total << std::endl;
    std::cout << "Empirical volume failure rate: " << failure_rate << std::endl;
    std::cout << "Final calibrated q_s (start lag margin): " << calibrator.get_q_s() << " s" << std::endl;
    std::cout << "Final calibrated q_e (cutoff margin): " << calibrator.get_q_e() << " s" << std::endl;
    std::cout << "Final calibrated q_r (rate degradation): " << calibrator.get_q_r() * 100.0 << " %" << std::endl;

    // Lemma 1 guarantee: failure rate must be closely bounded near alpha_link
    assert(failure_rate <= cfg.alpha_target_total + 0.04);
    std::cout << "[PASS] ACI Calibrator correctly bounds non-stationary contact failure!" << std::endl;

    return 0;
}
