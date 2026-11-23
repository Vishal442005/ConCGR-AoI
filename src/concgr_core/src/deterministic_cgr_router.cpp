#include "concgr_core/deterministic_cgr_router.hpp"
#include <queue>
#include <algorithm>

namespace concgr {

DeterministicCgrRouter::DeterministicCgrRouter(uint32_t node_id, const std::vector<ContactNominal>& nominal_plan)
    : DeterministicCgrRouter(node_id, nominal_plan, Config{})
{
}


DeterministicCgrRouter::DeterministicCgrRouter(
    uint32_t node_id, const std::vector<ContactNominal>& nominal_plan, const Config& config)
    : node_id_(node_id), config_(config), nominal_plan_(nominal_plan)
{
}

void DeterministicCgrRouter::store_bundle(const Bundle& bundle) {
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

void DeterministicCgrRouter::on_bundle_generated(const Bundle& bundle, double current_time) {
    (void)current_time;
    store_bundle(bundle);
}

void DeterministicCgrRouter::on_bundle_received(const Bundle& bundle, double current_time) {
    (void)current_time;
    for (const auto& b : queue_) {
        if (b.id == bundle.id) return;
    }
    store_bundle(bundle);
}

void DeterministicCgrRouter::on_contact_start(
    uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps)
{
    (void)current_time;
    (void)rate_bps;
    active_contacts_[peer_id] = contact_id;
}

void DeterministicCgrRouter::on_contact_end(
    uint32_t contact_id, uint32_t peer_id, double current_time)
{
    (void)contact_id;
    (void)current_time;
    active_contacts_.erase(peer_id);
}

struct CgrSearchNode {
    uint32_t node{0};
    double earliest_time{0.0};
    std::vector<RouteHop> hops;

    bool operator>(const CgrSearchNode& other) const {
        return earliest_time > other.earliest_time;
    }
};

std::vector<RouteHop> DeterministicCgrRouter::compute_nominal_dijkstra(
    uint32_t src, uint32_t dst, double start_time, size_t bundle_size_bytes) const
{
    if (src == dst) return {};

    double bundle_bits = static_cast<double>(bundle_size_bytes) * 8.0;
    std::priority_queue<CgrSearchNode, std::vector<CgrSearchNode>, std::greater<CgrSearchNode>> pq;
    std::unordered_map<uint32_t, double> best_time;

    CgrSearchNode init;
    init.node = src;
    init.earliest_time = start_time;
    pq.push(init);
    best_time[src] = start_time;

    while (!pq.empty()) {
        CgrSearchNode curr = pq.top();
        pq.pop();

        if (curr.node == dst) {
            return curr.hops;
        }

        if (curr.earliest_time > best_time[curr.node] + 1e-6) {
            continue;
        }

        // Uncalibrated nominal search: uses raw [t_start, t_end]
        for (const auto& c : nominal_plan_) {
            if (c.tx_id != curr.node) continue;
            if (c.t_end <= curr.earliest_time) continue;

            double tx_start = std::max(curr.earliest_time, c.t_start);
            double tx_dur = bundle_bits / c.rate_bps;
            double tx_end = tx_start + tx_dur;

            if (tx_end > c.t_end) continue; // Doesn't fit in nominal window

            uint32_t nxt = c.rx_id;
            auto it = best_time.find(nxt);
            if (it == best_time.end() || tx_end < it->second) {
                best_time[nxt] = tx_end;

                CgrSearchNode nxt_node;
                nxt_node.node = nxt;
                nxt_node.earliest_time = tx_end;
                nxt_node.hops = curr.hops;

                RouteHop h;
                h.contact_id = c.id;
                h.tx_id = c.tx_id;
                h.rx_id = c.rx_id;
                h.t_tx_start = tx_start;
                h.t_tx_end = tx_end;
                h.rate_bps = c.rate_bps;
                nxt_node.hops.push_back(h);

                pq.push(nxt_node);
            }
        }
    }

    return {};
}

std::vector<ForwardDecision> DeterministicCgrRouter::step(double current_time) {
    std::vector<ForwardDecision> decisions;
    if (active_contacts_.empty() || queue_.empty()) {
        return decisions;
    }

    std::unordered_map<uint32_t, std::vector<RouteHop>> route_cache;

    for (const auto& bundle : queue_) {
        uint64_t bid = bundle.id;

        auto c_it = route_cache.find(bundle.dst);
        if (c_it == route_cache.end()) {
            route_cache[bundle.dst] = compute_nominal_dijkstra(node_id_, bundle.dst, current_time, bundle.size_bytes);
            c_it = route_cache.find(bundle.dst);
        }

        const auto& hops = c_it->second;
        if (hops.empty()) {
            continue;
        }

        const auto& next_hop = hops.front();
        uint32_t peer_id = next_hop.rx_id;

        // If the calculated contact is currently active, forward!
        auto act_it = active_contacts_.find(peer_id);
        if (act_it != active_contacts_.end()) {
            std::string key = std::to_string(peer_id) + "_" + std::to_string(bid);
            if (sent_history_.find(key) == sent_history_.end()) {
                ForwardDecision d;
                d.bundle_id = bid;
                d.next_hop_uav_id = peer_id;
                d.contact_id = act_it->second;
                d.is_custody_transfer = true; // Single-copy custody forwarding
                decisions.push_back(d);

                sent_history_.insert(key);
                total_forwardings_++;
            }
        }
    }

    return decisions;
}

void DeterministicCgrRouter::on_custody_ack(uint64_t bundle_id) {
    auto it = std::find_if(queue_.begin(), queue_.end(),
        [bundle_id](const Bundle& b) { return b.id == bundle_id; });
    if (it != queue_.end()) {
        current_buffer_bytes_ -= it->size_bytes;
        queue_.erase(it);
    }
}

} // namespace concgr
