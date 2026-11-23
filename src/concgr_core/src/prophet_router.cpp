#include "concgr_core/prophet_router.hpp"
#include <algorithm>

namespace concgr {

ProphetRouter::ProphetRouter(uint32_t node_id)
    : ProphetRouter(node_id, Config{})
{
}


ProphetRouter::ProphetRouter(uint32_t node_id, const Config& config)
    : node_id_(node_id), config_(config)
{
}

double ProphetRouter::get_predictability(uint32_t dest_id) const {
    auto it = p_table_.find(dest_id);
    if (it != p_table_.end()) {
        return it->second;
    }
    return 0.0;
}

void ProphetRouter::update_predictability_on_contact(uint32_t peer_id, double current_time) {
    // 1. Aging predictability table
    if (current_time > last_aging_time_ + 30.0) {
        for (auto& [dest, p_val] : p_table_) {
            p_val *= config_.gamma_aging;
        }
        last_aging_time_ = current_time;
    }

    // 2. Encounter update: P(a, b) = P(a, b)_old + (1 - P(a, b)_old) * P_encounter
    double p_old = get_predictability(peer_id);
    double p_new = p_old + (1.0 - p_old) * config_.p_encounter;
    p_table_[peer_id] = std::clamp(p_new, 0.0, 1.0);
}

void ProphetRouter::store_bundle(const Bundle& bundle) {
    if (bundle.size_bytes > config_.max_buffer_bytes) {
        total_evictions_++;
        return;
    }

    while (current_buffer_bytes_ + bundle.size_bytes > config_.max_buffer_bytes && !queue_.empty()) {
        current_buffer_bytes_ -= queue_.front().size_bytes;
        queue_.erase(queue_.begin());
        total_evictions_++;
    }

    queue_.push_back(bundle);
    current_buffer_bytes_ += bundle.size_bytes;
}

void ProphetRouter::on_bundle_generated(const Bundle& bundle, double current_time) {
    (void)current_time;
    store_bundle(bundle);
}

void ProphetRouter::on_bundle_received(const Bundle& bundle, double current_time) {
    (void)current_time;
    for (const auto& b : queue_) {
        if (b.id == bundle.id) return;
    }
    store_bundle(bundle);
}

void ProphetRouter::on_contact_start(
    uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps)
{
    (void)rate_bps;
    active_contacts_[peer_id] = contact_id;
    update_predictability_on_contact(peer_id, current_time);
}

void ProphetRouter::on_contact_end(
    uint32_t contact_id, uint32_t peer_id, double current_time)
{
    (void)contact_id;
    (void)current_time;
    active_contacts_.erase(peer_id);
}

std::vector<ForwardDecision> ProphetRouter::step(double current_time) {
    (void)current_time;
    std::vector<ForwardDecision> decisions;

    for (const auto& [peer_id, contact_id] : active_contacts_) {
        for (const auto& bundle : queue_) {
            uint64_t bid = bundle.id;
            std::string key = std::to_string(peer_id) + "_" + std::to_string(bid);
            if (sent_history_.find(key) != sent_history_.end()) {
                continue;
            }

            // Direct delivery to destination
            if (peer_id == bundle.dst) {
                ForwardDecision d;
                d.bundle_id = bid;
                d.next_hop_uav_id = peer_id;
                d.contact_id = contact_id;
                d.is_custody_transfer = true;
                decisions.push_back(d);

                sent_history_.insert(key);
                total_forwardings_++;
                continue;
            }

            // PRoPHET forwarding rule: forward if peer has higher predictability towards destination
            // In a distributed setup without routing summary vectors, a peer that has recently
            // encountered destination has P(peer, dst) > P(me, dst). We simulate this by comparing
            // our own predictability towards dst vs peer.
            double my_p = get_predictability(bundle.dst);
            double peer_p = get_predictability(peer_id); // proxy metric for forwarder quality

            if (peer_p > my_p || my_p < 0.1) {
                ForwardDecision d;
                d.bundle_id = bid;
                d.next_hop_uav_id = peer_id;
                d.contact_id = contact_id;
                d.is_custody_transfer = false;
                decisions.push_back(d);

                sent_history_.insert(key);
                total_forwardings_++;
            }
        }
    }

    return decisions;
}

void ProphetRouter::on_custody_ack(uint64_t bundle_id) {
    auto it = std::find_if(queue_.begin(), queue_.end(),
        [bundle_id](const Bundle& b) { return b.id == bundle_id; });
    if (it != queue_.end()) {
        current_buffer_bytes_ -= it->size_bytes;
        queue_.erase(it);
    }
}

} // namespace concgr
