#pragma once

#include "concgr_core/router_interface.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace concgr {

class ConnectedFanetRouter : public DtnRouterBase {
public:
    struct Config {
        size_t max_buffer_bytes{8 * 1024 * 1024};
        double drop_timeout_sec{5.0}; // Drops if no connected path within 5s
    };

    explicit ConnectedFanetRouter(uint32_t node_id);
    ConnectedFanetRouter(uint32_t node_id, const Config& config);

    std::string get_protocol_name() const override { return "Connected-FANET (OLSR)"; }

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
    uint32_t node_id_;
    Config config_;
    size_t current_buffer_bytes_{0};
    size_t total_evictions_{0};
    size_t total_forwardings_{0};

    std::vector<Bundle> queue_;
    std::unordered_map<uint32_t, uint32_t> active_contacts_;
    std::unordered_set<std::string> sent_history_;
};

} // namespace concgr
