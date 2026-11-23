#include "concgr_core/connected_fanet_router.hpp"
#include <algorithm>

namespace concgr {

ConnectedFanetRouter::ConnectedFanetRouter(uint32_t node_id)
    : ConnectedFanetRouter(node_id, Config{})
{
}


ConnectedFanetRouter::ConnectedFanetRouter(uint32_t node_id, const Config& config)
    : node_id_(node_id), config_(config)
{
}

void ConnectedFanetRouter::on_bundle_generated(const Bundle& bundle, double current_time) {
    (void)current_time;
    queue_.push_back(bundle);
    current_buffer_bytes_ += bundle.size_bytes;
}

void ConnectedFanetRouter::on_bundle_received(const Bundle& bundle, double current_time) {
    (void)current_time;
    for (const auto& b : queue_) {
        if (b.id == bundle.id) return;
    }
    queue_.push_back(bundle);
    current_buffer_bytes_ += bundle.size_bytes;
}

void ConnectedFanetRouter::on_contact_start(
    uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps)
{
    (void)current_time;
    (void)rate_bps;
    active_contacts_[peer_id] = contact_id;
}

void ConnectedFanetRouter::on_contact_end(
    uint32_t contact_id, uint32_t peer_id, double current_time)
{
    (void)contact_id;
    (void)current_time;
    active_contacts_.erase(peer_id);
}

std::vector<ForwardDecision> ConnectedFanetRouter::step(double current_time) {
    std::vector<ForwardDecision> decisions;

    // In connected FANET routing, bundles only forward if there is an active link to next hop
    // If no path to destination exists, bundle times out and is dropped immediately
    auto it = queue_.begin();
    while (it != queue_.end()) {
        uint64_t bid = it->id;
        uint32_t dst = it->dst;

        // Direct link active to destination?
        auto act_it = active_contacts_.find(dst);
        if (act_it != active_contacts_.end()) {
            std::string key = std::to_string(dst) + "_" + std::to_string(bid);
            if (sent_history_.find(key) == sent_history_.end()) {
                ForwardDecision d;
                d.bundle_id = bid;
                d.next_hop_uav_id = dst;
                d.contact_id = act_it->second;
                d.is_custody_transfer = true;
                decisions.push_back(d);

                sent_history_.insert(key);
                total_forwardings_++;
            }
            ++it;
        } else {
            // Drop packet if disconnected longer than drop_timeout_sec
            if (current_time - it->t_gen > config_.drop_timeout_sec) {
                current_buffer_bytes_ -= it->size_bytes;
                it = queue_.erase(it);
                total_evictions_++;
            } else {
                ++it;
            }
        }
    }

    return decisions;
}

void ConnectedFanetRouter::on_custody_ack(uint64_t bundle_id) {
    auto it = std::find_if(queue_.begin(), queue_.end(),
        [bundle_id](const Bundle& b) { return b.id == bundle_id; });
    if (it != queue_.end()) {
        current_buffer_bytes_ -= it->size_bytes;
        queue_.erase(it);
    }
}

} // namespace concgr
