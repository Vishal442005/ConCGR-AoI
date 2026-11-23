#include "concgr_core/spray_and_wait_router.hpp"
#include <cmath>
#include <algorithm>

namespace concgr {

SprayAndWaitRouter::SprayAndWaitRouter(uint32_t node_id)
    : SprayAndWaitRouter(node_id, Config{})
{
}


SprayAndWaitRouter::SprayAndWaitRouter(uint32_t node_id, const Config& config)
    : node_id_(node_id), config_(config)
{
}

void SprayAndWaitRouter::store_bundle(Bundle bundle) {
    if (bundle.size_bytes > config_.max_buffer_bytes) {
        total_evictions_++;
        return;
    }

    while (current_buffer_bytes_ + bundle.size_bytes > config_.max_buffer_bytes && !queue_.empty()) {
        uint64_t evict_id = queue_.front().id;
        current_buffer_bytes_ -= queue_.front().size_bytes;
        queue_.erase(queue_.begin());
        copies_remaining_.erase(evict_id);
        total_evictions_++;
    }

    uint64_t id = bundle.id;
    if (copies_remaining_.find(id) == copies_remaining_.end()) {
        copies_remaining_[id] = (bundle.replication_quota > 0) ? bundle.replication_quota : config_.initial_copies;
    }

    queue_.push_back(bundle);
    current_buffer_bytes_ += bundle.size_bytes;
}

void SprayAndWaitRouter::on_bundle_generated(const Bundle& bundle, double current_time) {
    (void)current_time;
    Bundle b = bundle;
    b.replication_quota = config_.initial_copies;
    store_bundle(b);
}

void SprayAndWaitRouter::on_bundle_received(const Bundle& bundle, double current_time) {
    (void)current_time;
    for (const auto& b : queue_) {
        if (b.id == bundle.id) return;
    }
    store_bundle(bundle);
}

void SprayAndWaitRouter::on_contact_start(
    uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps)
{
    (void)current_time;
    (void)rate_bps;
    active_contacts_[peer_id] = contact_id;
}

void SprayAndWaitRouter::on_contact_end(
    uint32_t contact_id, uint32_t peer_id, double current_time)
{
    (void)contact_id;
    (void)current_time;
    active_contacts_.erase(peer_id);
}

std::vector<ForwardDecision> SprayAndWaitRouter::step(double current_time) {
    (void)current_time;
    std::vector<ForwardDecision> decisions;

    for (const auto& [peer_id, contact_id] : active_contacts_) {
        for (auto& bundle : queue_) {
            uint64_t bid = bundle.id;
            std::string key = std::to_string(peer_id) + "_" + std::to_string(bid);
            if (sent_history_.find(key) != sent_history_.end()) {
                continue;
            }

            uint32_t copies = copies_remaining_[bid];

            // Case 1: Peer IS the destination -> Direct delivery (Wait phase)
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

            // Case 2: Spray phase (L > 1) -> Binary spray: give half to peer
            if (copies > 1) {
                uint32_t handed_copies = copies / 2;
                uint32_t kept_copies = copies - handed_copies;

                ForwardDecision d;
                d.bundle_id = bid;
                d.next_hop_uav_id = peer_id;
                d.contact_id = contact_id;
                d.is_custody_transfer = false;
                decisions.push_back(d);

                copies_remaining_[bid] = kept_copies;
                bundle.replication_quota = kept_copies;
                sent_history_.insert(key);
                total_forwardings_++;
            }
        }
    }

    return decisions;
}

void SprayAndWaitRouter::on_custody_ack(uint64_t bundle_id) {
    auto it = std::find_if(queue_.begin(), queue_.end(),
        [bundle_id](const Bundle& b) { return b.id == bundle_id; });
    if (it != queue_.end()) {
        current_buffer_bytes_ -= it->size_bytes;
        queue_.erase(it);
        copies_remaining_.erase(bundle_id);
    }
}

} // namespace concgr
