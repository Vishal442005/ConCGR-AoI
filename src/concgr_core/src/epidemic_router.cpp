#include "concgr_core/epidemic_router.hpp"

namespace concgr {

EpidemicRouter::EpidemicRouter(uint32_t node_id)
    : EpidemicRouter(node_id, Config{})
{
}


EpidemicRouter::EpidemicRouter(uint32_t node_id, const Config& config)
    : node_id_(node_id), config_(config)
{
}

void EpidemicRouter::store_bundle(const Bundle& bundle) {
    // If incoming bundle is larger than entire buffer, reject
    if (bundle.size_bytes > config_.max_buffer_bytes) {
        total_evictions_++;
        return;
    }

    // Standard FIFO eviction: drop oldest bundles first until space is available
    while (current_buffer_bytes_ + bundle.size_bytes > config_.max_buffer_bytes && !queue_.empty()) {
        current_buffer_bytes_ -= queue_.front().size_bytes;
        queue_.erase(queue_.begin());
        total_evictions_++;
    }

    queue_.push_back(bundle);
    current_buffer_bytes_ += bundle.size_bytes;
}

void EpidemicRouter::on_bundle_generated(const Bundle& bundle, double current_time) {
    (void)current_time;
    store_bundle(bundle);
}

void EpidemicRouter::on_bundle_received(const Bundle& bundle, double current_time) {
    (void)current_time;
    // Don't store if already in queue
    for (const auto& b : queue_) {
        if (b.id == bundle.id) return;
    }
    store_bundle(bundle);
}

void EpidemicRouter::on_contact_start(
    uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps)
{
    (void)current_time;
    (void)rate_bps;
    active_contacts_[peer_id] = contact_id;
}

void EpidemicRouter::on_contact_end(
    uint32_t contact_id, uint32_t peer_id, double current_time)
{
    (void)contact_id;
    (void)current_time;
    active_contacts_.erase(peer_id);
}

std::vector<ForwardDecision> EpidemicRouter::step(double current_time) {
    (void)current_time;
    std::vector<ForwardDecision> decisions;

    // Epidemic: For every active contact peer, replicate all unsent bundles
    for (const auto& [peer_id, contact_id] : active_contacts_) {
        for (const auto& bundle : queue_) {
            std::string key = std::to_string(peer_id) + "_" + std::to_string(bundle.id);
            if (sent_history_.find(key) == sent_history_.end()) {
                ForwardDecision decision;
                decision.bundle_id = bundle.id;
                decision.next_hop_uav_id = peer_id;
                decision.contact_id = contact_id;
                decision.is_custody_transfer = false; // Replicated (multi-copy)
                decisions.push_back(decision);

                sent_history_.insert(key);
                total_forwardings_++;
            }
        }
    }

    return decisions;
}

void EpidemicRouter::on_custody_ack(uint64_t bundle_id) {
    auto it = std::find_if(queue_.begin(), queue_.end(),
        [bundle_id](const Bundle& b) { return b.id == bundle_id; });
    if (it != queue_.end()) {
        current_buffer_bytes_ -= it->size_bytes;
        queue_.erase(it);
    }
}

} // namespace concgr
