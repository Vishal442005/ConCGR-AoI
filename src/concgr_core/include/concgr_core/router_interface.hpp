#pragma once

#include "concgr_core/types.hpp"
#include <vector>
#include <memory>
#include <string>

namespace concgr {

struct ForwardDecision {
    uint64_t bundle_id{0};
    uint32_t next_hop_uav_id{0};
    uint32_t contact_id{0};
    bool is_custody_transfer{false};
};

class DtnRouterBase {
public:
    virtual ~DtnRouterBase() = default;

    virtual std::string get_protocol_name() const = 0;

    // Called when a new bundle is generated locally by sensor topic
    virtual void on_bundle_generated(const Bundle& bundle, double current_time) = 0;

    // Called when a bundle is received over radio from a peer UAV
    virtual void on_bundle_received(const Bundle& bundle, double current_time) = 0;

    // Called when a contact window starts
    virtual void on_contact_start(
        uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps) = 0;

    // Called when a contact window terminates
    virtual void on_contact_end(
        uint32_t contact_id, uint32_t peer_id, double current_time) = 0;

    // Periodic routing decision step: returns list of bundles to transmit across active contacts
    virtual std::vector<ForwardDecision> step(double current_time) = 0;

    // Acknowledgment received: bundle successfully arrived at destination
    virtual void on_custody_ack(uint64_t bundle_id) = 0;

    // Get current buffer size in bytes
    virtual size_t get_buffer_bytes() const = 0;

    // Get queue of stored bundles
    virtual const std::vector<Bundle>& get_queue() const = 0;

    // Router statistics
    virtual size_t get_total_evictions() const = 0;
    virtual size_t get_total_forwardings() const = 0;
};

} // namespace concgr
