#pragma once

#include "concgr_core/router_interface.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace concgr {

class DeterministicCgrRouter : public DtnRouterBase {
public:
    struct Config {
        size_t max_buffer_bytes{8 * 1024 * 1024};
    };

    DeterministicCgrRouter(uint32_t node_id, const std::vector<ContactNominal>& nominal_plan);
    DeterministicCgrRouter(uint32_t node_id, const std::vector<ContactNominal>& nominal_plan, const Config& config);

    std::string get_protocol_name() const override { return "Deterministic-CGR"; }

    void on_bundle_generated(const Bundle& bundle, double current_time) override;
    void on_bundle_received(const Bundle& bundle, double current_time) override;

    void on_contact_start(uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps) override;
    void on_contact_end(uint32_t contact_id, uint32_t peer_id, double current_time) override;

    std::vector<ForwardDecision> step(double current_time) override;
    void on_custody_ack(uint64_t bundle_id) override;

    size_t get_buffer_bytes() const override { return current_buffer_bytes_; }
    const std::vector<Bundle>& get_queue() const override { return queue_; }
    size_t get_total_evictions() const override { return total_evictions_; }
    size_t get_total_forwardings() const override { return total_forwardings_; }

private:
    void store_bundle(const Bundle& bundle);
    // Standard Dijkstra over uncalibrated nominal contact plan
    std::vector<RouteHop> compute_nominal_dijkstra(
        uint32_t src, uint32_t dst, double start_time, size_t bundle_size_bytes) const;

    uint32_t node_id_;
    Config config_;
    size_t current_buffer_bytes_{0};
    size_t total_evictions_{0};
    size_t total_forwardings_{0};

    std::vector<ContactNominal> nominal_plan_;
    std::vector<Bundle> queue_;
    std::unordered_map<uint32_t, uint32_t> active_contacts_;
    std::unordered_set<std::string> sent_history_;
};

} // namespace concgr
