#include "concgr_core/aci_calibrator.hpp"
#include <numeric>
#include <algorithm>

namespace concgr {

AciCalibrator::AciCalibrator()
    : AciCalibrator(Config{})
{
}

AciCalibrator::AciCalibrator(const Config& config)
    : config_(config),
      alpha_s_target_(config.alpha_target_total / 3.0),
      alpha_e_target_(config.alpha_target_total / 3.0),
      alpha_r_target_(config.alpha_target_total / 3.0),
      alpha_s_(alpha_s_target_),
      alpha_e_(alpha_e_target_),
      alpha_r_(alpha_r_target_)
{
}

double AciCalibrator::compute_weighted_quantile(const std::deque<double>& residuals, double miscoverage) const {
    if (residuals.empty()) {
        return 0.0;
    }
    const size_t n = residuals.size();
    std::vector<std::pair<double, double>> weighted_pairs(n);
    double total_weight = 0.0;

    for (size_t i = 0; i < n; ++i) {
        // Most recent element (i = n-1) has weight beta^0 = 1.0
        double w = std::pow(config_.beta, static_cast<double>(n - 1 - i));
        weighted_pairs[i] = {residuals[i], w};
        total_weight += w;
    }

    std::sort(weighted_pairs.begin(), weighted_pairs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    double target_cum_weight = (1.0 - miscoverage) * total_weight;
    double cum_weight = 0.0;

    for (const auto& pair : weighted_pairs) {
        cum_weight += pair.second;
        if (cum_weight >= target_cum_weight) {
            return pair.first;
        }
    }

    return weighted_pairs.back().first;
}

void AciCalibrator::record_contact_feedback(const ContactNominal& nominal, const ContactRealized& realized) {
    // 1. Compute asymmetric residuals (Eq. 2)
    double e_s = std::max(0.0, realized.t_start_real - nominal.t_start);
    double e_e = std::max(0.0, nominal.t_end - realized.t_end_real);
    double e_r = 0.0;
    if (nominal.rate_bps > 1e-6) {
        e_r = std::max(0.0, 1.0 - (realized.rate_bps_real / nominal.rate_bps));
    }
    e_r = std::min(1.0, e_r);

    // 2. Evaluate step miscoverage errors err_m(t) against current quantiles
    double err_s = (e_s > q_s_) ? 1.0 : 0.0;
    double err_e = (e_e > q_e_) ? 1.0 : 0.0;
    double err_r = (e_r > q_r_) ? 1.0 : 0.0;

    // 3. Update Adaptive Conformal Inference miscoverage parameters (Eq. 5)
    alpha_s_ = std::clamp(alpha_s_ + config_.gamma_s * (alpha_s_target_ - err_s),
                          config_.alpha_min, config_.alpha_max);
    alpha_e_ = std::clamp(alpha_e_ + config_.gamma_e * (alpha_e_target_ - err_e),
                          config_.alpha_min, config_.alpha_max);
    alpha_r_ = std::clamp(alpha_r_ + config_.gamma_r * (alpha_r_target_ - err_r),
                          config_.alpha_min, config_.alpha_max);

    // 4. Append to sliding history buffer
    history_s_.push_back(e_s);
    history_e_.push_back(e_e);
    history_r_.push_back(e_r);

    if (history_s_.size() > config_.window_size) {
        history_s_.pop_front();
        history_e_.pop_front();
        history_r_.pop_front();
    }

    // 5. Update calibrated empirical quantiles (Eq. 4)
    q_s_ = compute_weighted_quantile(history_s_, alpha_s_);
    q_e_ = compute_weighted_quantile(history_e_, alpha_e_);
    q_r_ = compute_weighted_quantile(history_r_, alpha_r_);
}

ConformalContact AciCalibrator::calibrate_contact(const ContactNominal& nominal) const {
    ConformalContact conf;
    conf.id = nominal.id;
    conf.tx_id = nominal.tx_id;
    conf.rx_id = nominal.rx_id;

    // Shrunken interval [t_start + q_s, t_end - q_e] (Eq. 6)
    conf.t_start_conf = nominal.t_start + q_s_;
    conf.t_end_conf = nominal.t_end - q_e_;

    // Degraded transmission rate r * (1 - q_r)
    conf.effective_rate_bps = std::max(0.0, nominal.rate_bps * (1.0 - q_r_));

    // Usable effective volume V_conf = max(0, t_end_conf - t_start_conf) * effective_rate
    double conf_duration = std::max(0.0, conf.t_end_conf - conf.t_start_conf);
    conf.effective_volume_bits = conf_duration * conf.effective_rate_bps;
    conf.booked_volume_bits = 0.0;

    // Admission rule: positive duration and volume above threshold
    conf.is_admitted = (conf.t_end_conf > conf.t_start_conf) &&
                       (conf.effective_volume_bits >= config_.min_usable_volume_bits);

    return conf;
}

} // namespace concgr
