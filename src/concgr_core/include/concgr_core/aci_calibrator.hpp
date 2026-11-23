#pragma once

#include "concgr_core/types.hpp"
#include <deque>
#include <cmath>
#include <vector>

namespace concgr {

class AciCalibrator {
public:
    struct Config {
        size_t window_size{100};
        double beta{0.98};              // Exponential decay factor
        double gamma_s{0.05};           // ACI learning step for start lag
        double gamma_e{0.05};           // ACI learning step for early cutoff
        double gamma_r{0.05};           // ACI learning step for rate degradation
        double alpha_target_total{0.05}; // Total link risk budget alpha_link
        double alpha_min{0.005};
        double alpha_max{0.50};
        double min_usable_volume_bits{8192.0}; // Minimum volume to admit contact
    };

    AciCalibrator();
    explicit AciCalibrator(const Config& config);

    // Update ACI state with a newly completed realized contact vs nominal contact
    void record_contact_feedback(const ContactNominal& nominal, const ContactRealized& realized);

    // Conformalize a candidate nominal contact using current quantile margins
    ConformalContact calibrate_contact(const ContactNominal& nominal) const;

    // Get current quantiles and error rates
    double get_q_s() const { return q_s_; }
    double get_q_e() const { return q_e_; }
    double get_q_r() const { return q_r_; }
    double get_alpha_s() const { return alpha_s_; }
    double get_alpha_e() const { return alpha_e_; }
    double get_alpha_r() const { return alpha_r_; }

    size_t get_history_count() const { return history_s_.size(); }

private:
    double compute_weighted_quantile(const std::deque<double>& residuals, double miscoverage) const;

    Config config_;
    double alpha_s_target_{0.0167};
    double alpha_e_target_{0.0167};
    double alpha_r_target_{0.0167};

    double alpha_s_{0.0167};
    double alpha_e_{0.0167};
    double alpha_r_{0.0167};

    double q_s_{0.0};
    double q_e_{0.0};
    double q_r_{0.0};

    std::deque<double> history_s_;
    std::deque<double> history_e_;
    std::deque<double> history_r_;
};

} // namespace concgr
