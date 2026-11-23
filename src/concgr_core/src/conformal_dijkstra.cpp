#include "concgr_core/conformal_dijkstra.hpp"
#include <queue>
#include <algorithm>
#include <limits>

namespace concgr {

ConformalDijkstraRouter::ConformalDijkstraRouter()
    : ConformalDijkstraRouter(Config{})
{
}

ConformalDijkstraRouter::ConformalDijkstraRouter(const Config& config)
    : config_(config),
      rep_engine_(ReplicationEngine::Config{config.k_max, config.alpha_path})
{
}

struct SearchNode {
    uint32_t node_id{0};
    double earliest_time{0.0};
    uint32_t hops{0};
    std::vector<RouteHop> path_hops;

    bool operator>(const SearchNode& other) const {
        return earliest_time > other.earliest_time;
    }
};

RoutePlan ConformalDijkstraRouter::compute_route(
    uint32_t src_id,
    uint32_t dst_id,
    const Bundle& bundle,
    const std::vector<ConformalContact>& contact_plan,
    double current_time
) const {
    RoutePlan result;
    result.found = false;

    if (src_id == dst_id) {
        result.found = true;
        result.arrival_time = current_time;
        result.peak_aoi = std::max(0.0, current_time - bundle.t_gen);
        result.replication_quota = 1;
        return result;
    }

    double bundle_bits = static_cast<double>(bundle.size_bytes) * 8.0;

    // Min-priority queue ordered by earliest arrival time
    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> pq;

    // Best known arrival time at each node
    std::unordered_map<uint32_t, double> best_arrival;

    SearchNode start_node;
    start_node.node_id = src_id;
    start_node.earliest_time = std::max(current_time, bundle.t_gen);
    start_node.hops = 0;
    pq.push(start_node);
    best_arrival[src_id] = start_node.earliest_time;

    while (!pq.empty()) {
        SearchNode curr = pq.top();
        pq.pop();

        // If we reached the destination, check if this is optimal
        if (curr.node_id == dst_id) {
            result.found = true;
            result.hops = curr.path_hops;
            result.arrival_time = curr.earliest_time;
            result.peak_aoi = curr.earliest_time - bundle.t_gen;

            // Calculate per-hop risk allocation alpha_link = alpha_path / H_max
            double alpha_link = config_.alpha_path / static_cast<double>(std::max(1u, config_.max_hops));
            result.replication_quota = rep_engine_.compute_replication_quota(curr.hops, alpha_link);
            return result;
        }

        // If we already found a faster way to this node, skip
        if (curr.earliest_time > best_arrival[curr.node_id] + 1e-6) {
            continue;
        }

        // Stop expanding if hop limit reached
        if (curr.hops >= config_.max_hops) {
            continue;
        }

        // Search outgoing admitted conformal contacts
        for (const auto& contact : contact_plan) {
            if (!contact.is_admitted || contact.tx_id != curr.node_id) {
                continue;
            }

            // Contact must have sufficient residual volume
            double residual_bits = contact.effective_volume_bits - contact.booked_volume_bits;
            if (residual_bits < bundle_bits) {
                continue;
            }

            // Causal feasibility: contact must end after current bundle arrival time
            if (contact.t_end_conf <= curr.earliest_time) {
                continue;
            }

            // Earliest possible transmission start
            double tx_start = std::max(curr.earliest_time, contact.t_start_conf);

            if (contact.effective_rate_bps <= 1e-3) {
                continue;
            }

            // Transmission duration
            double tx_duration = bundle_bits / contact.effective_rate_bps;
            double tx_end = tx_start + tx_duration;

            // Must finish before contact conformal early cutoff
            if (tx_end > contact.t_end_conf) {
                continue;
            }

            // Peak-AoI deadline check: tx_end - bundle.t_gen <= D_b
            if (tx_end - bundle.t_gen > bundle.deadline_sec) {
                continue;
            }

            // Check if this gives a faster arrival at receiver
            uint32_t next_node = contact.rx_id;
            auto it = best_arrival.find(next_node);
            if (it == best_arrival.end() || tx_end < it->second) {
                best_arrival[next_node] = tx_end;

                SearchNode next_search;
                next_search.node_id = next_node;
                next_search.earliest_time = tx_end;
                next_search.hops = curr.hops + 1;
                next_search.path_hops = curr.path_hops;

                RouteHop hop;
                hop.contact_id = contact.id;
                hop.tx_id = contact.tx_id;
                hop.rx_id = contact.rx_id;
                hop.t_tx_start = tx_start;
                hop.t_tx_end = tx_end;
                hop.rate_bps = contact.effective_rate_bps;
                next_search.path_hops.push_back(hop);

                pq.push(next_search);
            }
        }
    }

    return result;
}

} // namespace concgr
