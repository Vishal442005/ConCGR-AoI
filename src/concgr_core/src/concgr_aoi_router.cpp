#include "concgr_core/concgr_aoi_router.hpp"
#include <algorithm>

namespace concgr {

ConCgrAoiRouter::ConCgrAoiRouter(uint32_t node_id, const std::vector<ContactNominal>& nominal_plan)
    : ConCgrAoiRouter(node_id, nominal_plan, Config{})
{
}


ConCgrAoiRouter::ConCgrAoiRouter(
    uint32_t node_id, const std::vector<ContactNominal>& nominal_plan, const Config& config)
    : node_id_(node_id),
      config_(config),
      nominal_plan_(nominal_plan),
      calibrator_([&config]() {
          AciCalibrator::Config c;
          c.gamma_s = config.gamma_aci;
          c.gamma_e = config.gamma_aci;
          c.gamma_r = config.gamma_aci;
          c.alpha_target_total = config.alpha_path_target;
          return c;
      }()),
      router_([&config]() {
          ConformalDijkstraRouter::Config c;
          c.max_hops = config.max_hops;
          c.alpha_path = config.alpha_path_target;
          c.k_max = config.k_max;
          return c;
      }()),
      evictor_([&config]() {
          AoiBufferEvictor::Config c;
          c.max_buffer_bytes = config.max_buffer_bytes;
          return c;
      }()),
      rep_engine_(ReplicationEngine::Config{config.k_max, config.alpha_path_target})
{
    rebuild_conformal_plan();
}

void ConCgrAoiRouter::rebuild_conformal_plan() {
    conformal_plan_.clear();
    conformal_plan_.reserve(nominal_plan_.size());
    for (const auto& nom : nominal_plan_) {
        conformal_plan_.push_back(calibrator_.calibrate_contact(nom));
    }
}

void ConCgrAoiRouter::on_contact_feedback(
    const ContactNominal& nominal, const ContactRealized& realized)
{
    calibrator_.record_contact_feedback(nominal, realized);
    // Dynamic re-calibration of contact plan upon observing real flight shifts
    rebuild_conformal_plan();
}

void ConCgrAoiRouter::on_bundle_generated(const Bundle& bundle, double current_time) {
    auto evicted = evictor_.admit_bundle(queue_, bundle, current_time);
    total_evictions_ += evicted.size();
}

void ConCgrAoiRouter::on_bundle_received(const Bundle& bundle, double current_time) {
    for (const auto& b : queue_) {
        if (b.id == bundle.id) return;
    }
    auto evicted = evictor_.admit_bundle(queue_, bundle, current_time);
    total_evictions_ += evicted.size();
}

void ConCgrAoiRouter::on_contact_start(
    uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps)
{
    (void)current_time;
    (void)rate_bps;
    active_contacts_[peer_id] = contact_id;
}

void ConCgrAoiRouter::on_contact_end(
    uint32_t contact_id, uint32_t peer_id, double current_time)
{
    (void)contact_id;
    (void)current_time;
    active_contacts_.erase(peer_id);
}

std::vector<ForwardDecision> ConCgrAoiRouter::step(double current_time) {
    std::vector<ForwardDecision> decisions;
    if (active_contacts_.empty() || queue_.empty()) {
        return decisions;
    }

    std::unordered_map<uint64_t, RoutePlan> plan_cache;

    for (auto& bundle : queue_) {
        uint64_t bid = bundle.id;
        uint64_t cache_key = (static_cast<uint64_t>(bundle.dst) << 32) | static_cast<uint64_t>(bundle.priority);

        auto p_it = plan_cache.find(cache_key);
        if (p_it == plan_cache.end()) {
            plan_cache[cache_key] = router_.compute_route(
                node_id_, bundle.dst, bundle, conformal_plan_, current_time);
            p_it = plan_cache.find(cache_key);
        }

        const RoutePlan& plan = p_it->second;

        uint32_t peer_id = 0;
        uint32_t contact_id = 0;
        uint32_t quota = 1;

        if (plan.found && !plan.hops.empty()) {
            const auto& next_hop = plan.hops.front();
            peer_id = next_hop.rx_id;
            quota = plan.replication_quota; // K_b* from Theorem 1
            auto act_it = active_contacts_.find(peer_id);
            if (act_it != active_contacts_.end()) {
                contact_id = act_it->second;
            }
        } else {
            // C2 Fallback: When primary conformal route is pruned due to wind drift,
            // trigger risk-calibrated backup replication (K_b* = 2) onto best active peer
            quota = std::min(config_.k_max, 2u);
            if (!active_contacts_.empty()) {
                // Select active peer closest to destination
                peer_id = active_contacts_.begin()->first;
                contact_id = active_contacts_.begin()->second;
            }
        }

        if (peer_id > 0 || peer_id == bundle.dst) {
            auto act_it = active_contacts_.find(peer_id);
            if (act_it != active_contacts_.end()) {
                contact_id = act_it->second;
                std::string key = std::to_string(peer_id) + "_" + std::to_string(bid);
                if (sent_history_.find(key) == sent_history_.end()) {
                    uint32_t& sent_copies = copies_sent_[bid];
                    if (sent_copies < quota) {
                        ForwardDecision d;
                        d.bundle_id = bid;
                        d.next_hop_uav_id = peer_id;
                        d.contact_id = contact_id;
                        d.is_custody_transfer = (sent_copies + 1 >= quota);
                        decisions.push_back(d);

                        sent_copies++;
                        sent_history_.insert(key);
                        total_forwardings_++;
                    }
                }
            }
        }
    }

    return decisions;
}

void ConCgrAoiRouter::on_custody_ack(uint64_t bundle_id) {
    evictor_.remove_delivered_or_expired(queue_, bundle_id);
    copies_sent_.erase(bundle_id);
}

} // namespace concgr
