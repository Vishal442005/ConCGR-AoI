#pragma once

#include "concgr_core/router_interface.hpp"
#include "concgr_core/aci_calibrator.hpp"
#include "concgr_core/conformal_dijkstra.hpp"
#include "concgr_core/aoi_buffer_evictor.hpp"
#include "concgr_core/replication_engine.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace concgr {

class ConCgrAoiRouter : public DtnRouterBase {
public:
    struct Config {
        size_t max_buffer_bytes{8 * 1024 * 1024};
        double alpha_path_target{0.05};
        double gamma_aci{0.05};
        uint32_t k_max{3};
        uint32_t max_hops{6};
    };

    ConCgrAoiRouter(uint32_t node_id, const std::vector<ContactNominal>& nominal_plan);
    ConCgrAoiRouter(uint32_t node_id, const std::vector<ContactNominal>& nominal_plan, const Config& config);

    std::string get_protocol_name() const override { return "ConCGR-AoI (Proposed)"; }

    void on_bundle_generated(const Bundle& bundle, double current_time) override;
    void on_bundle_received(const Bundle& bundle, double current_time) override;

    void on_contact_start(uint32_t contact_id, uint32_t peer_id, double current_time, double rate_bps) override;
    void on_contact_end(uint32_t contact_id, uint32_t peer_id, double current_time) override;

    // Online ACI feedback from completed contact
    void on_contact_feedback(const ContactNominal& nominal, const ContactRealized& realized);

    std::vector<ForwardDecision> step(double current_time) override;
    void on_custody_ack(uint64_t bundle_id) override;

    size_t get_buffer_bytes() const override { return evictor_.get_current_bytes(); }
    const std::vector<Bundle>& get_queue() const override { return queue_; }
    size_t get_total_evictions() const override { return total_evictions_; }
    size_t get_total_forwardings() const override { return total_forwardings_; }

    const AciCalibrator& get_calibrator() const { return calibrator_; }

private:
    void rebuild_conformal_plan();

    uint32_t node_id_;
    Config config_;
    size_t total_evictions_{0};
    size_t total_forwardings_{0};

    std::vector<ContactNominal> nominal_plan_;
    std::vector<ConformalContact> conformal_plan_;

    AciCalibrator calibrator_;
    ConformalDijkstraRouter router_;
    AoiBufferEvictor evictor_;
    ReplicationEngine rep_engine_;

    std::vector<Bundle> queue_;
    std::unordered_map<uint32_t, uint32_t> active_contacts_;
    std::unordered_set<std::string> sent_history_;
    std::unordered_map<uint64_t, uint32_t> copies_sent_;
};

} // namespace concgr
