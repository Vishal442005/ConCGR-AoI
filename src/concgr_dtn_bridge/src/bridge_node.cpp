/*
 * bridge_node.cpp
 *
 * ROS 2 Humble ConCGR-AoI DTN Bridge Node for Autonomous UAVs (Research #2)
 *
 * Core functionalities:
 *   - Subscribes to local UAV ROS 2 topics (/uav_<id>/sensor_stream)
 *   - ConCGR-AoI mathematical routing engine with ACI calibrator and utility buffer eviction
 *   - UDP socket transport for inter-UAV / air-to-ground bundle forwarding
 *   - Direct Native DDS comparison channel with realistic mountain occlusion link blackout
 */

#include "rclcpp/rclcpp.hpp"
#include "concgr_msgs/msg/bundle_packet.hpp"
#include "concgr_core/types.hpp"
#include "concgr_core/concgr_aoi_router.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#pragma pack(push, 1)
struct DtnHeader {
    uint64_t bundle_id;
    uint32_t source_id;
    uint32_t destination_id;
    uint32_t priority;
    double t_gen;
    double deadline_sec;
    uint32_t payload_size;
    uint16_t chunk_idx;
    uint16_t total_chunks;
};
#pragma pack(pop)

struct StoredBundle {
    concgr::Bundle meta;
    std::vector<uint8_t> payload;
};

class ConcgrBridgeNode : public rclcpp::Node {
public:
    ConcgrBridgeNode() : Node("concgr_bridge_node") {
        this->declare_parameter<int>("uav_id", 1);
        this->declare_parameter<int>("sink_port", 5500);
        this->declare_parameter<std::string>("sink_ip", "127.0.0.1");
        this->declare_parameter<double>("buffer_cap_mb", 8.0);
        this->declare_parameter<double>("blackout_start_sec", 5.0);
        this->declare_parameter<double>("blackout_duration_sec", 20.0);

        uav_id_ = this->get_parameter("uav_id").as_int();
        sink_port_ = this->get_parameter("sink_port").as_int();
        sink_ip_ = this->get_parameter("sink_ip").as_string();
        double buffer_cap_mb = this->get_parameter("buffer_cap_mb").as_double();
        blackout_start_sec_ = this->get_parameter("blackout_start_sec").as_double();
        blackout_duration_sec_ = this->get_parameter("blackout_duration_sec").as_double();

        local_port_ = 5500 + uav_id_;

        // Initialize ConCGR-AoI router with nominal contact plan
        concgr::ConCgrAoiRouter::Config rcfg;
        rcfg.max_buffer_bytes = static_cast<size_t>(buffer_cap_mb * 1024.0 * 1024.0);
        rcfg.alpha_path_target = 0.05;

        // Nominal contact plan to Ground Station Sink
        std::vector<concgr::ContactNominal> plan;
        for (double t = 0.0; t <= 3600.0; t += 120.0) {
            concgr::ContactNominal c;
            c.id = plan.size() + 1;
            c.tx_id = uav_id_;
            c.rx_id = 0; // Sink
            c.t_start = t;
            c.t_end = t + 40.0;
            c.rate_bps = 6.0e6;
            plan.push_back(c);
        }

        router_ = std::make_unique<concgr::ConCgrAoiRouter>(uav_id_, plan, rcfg);

        init_udp_socket();

        std::string sub_topic = "/uav_" + std::to_string(uav_id_) + "/sensor_stream";
        sub_sensor_ = this->create_subscription<concgr_msgs::msg::BundlePacket>(
            sub_topic, 100,
            std::bind(&ConcgrBridgeNode::on_sensor_bundle, this, std::placeholders::_1)
        );

        // Native DDS comparison publisher to ground station
        pub_direct_dds_ = this->create_publisher<concgr_msgs::msg::BundlePacket>("/ground_station/direct_dds_stream", 100);

        // 20 Hz routing step timer
        timer_step_ = this->create_wall_timer(
            std::chrono::milliseconds(50),
            std::bind(&ConcgrBridgeNode::on_step, this)
        );

        // 100 Hz UDP receiver poll timer
        timer_recv_ = this->create_wall_timer(
            std::chrono::milliseconds(10),
            std::bind(&ConcgrBridgeNode::poll_incoming_udp, this)
        );

        start_time_ = this->now().seconds();

        RCLCPP_INFO(this->get_logger(),
            "ConCGR-AoI DTN Bridge initialized for UAV %d | Port: %d | Buffer: %.1f MB | Blackout: [%.1f s, %.1f s]",
            uav_id_, local_port_, buffer_cap_mb, blackout_start_sec_, blackout_start_sec_ + blackout_duration_sec_);
    }

    ~ConcgrBridgeNode() {
        if (sock_fd_ >= 0) close(sock_fd_);
    }

private:
    void init_udp_socket() {
        sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd_ < 0) {
            RCLCPP_ERROR(this->get_logger(), "Failed to create socket");
            return;
        }

        int flags = fcntl(sock_fd_, F_GETFL, 0);
        fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

        int sndbuf = 8 * 1024 * 1024;
        setsockopt(sock_fd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(local_port_);

        if (bind(sock_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            RCLCPP_ERROR(this->get_logger(), "Failed to bind to local port %d", local_port_);
        }
    }

    void on_sensor_bundle(const concgr_msgs::msg::BundlePacket::SharedPtr msg) {
        double now = this->now().seconds();
        double elapsed = now - start_time_;
        bool in_blackout = (elapsed >= blackout_start_sec_ && elapsed < (blackout_start_sec_ + blackout_duration_sec_));

        // 1. Native CycloneDDS direct stream path
        if (!in_blackout) {
            pub_direct_dds_->publish(*msg);
            RCLCPP_INFO(this->get_logger(),
                "[Native DDS Pass] Bundle #%lu forwarded directly (LoS open, t_gen: %.2f)", msg->bundle_id, msg->t_gen);
        } else {
            RCLCPP_WARN(this->get_logger(),
                "[Native DDS Blackout Loss] Bundle #%lu dropped by terrain occlusion (no buffer custody)", msg->bundle_id);
        }

        // 2. ConCGR-AoI DTN Bridge path
        concgr::Bundle b;
        b.id = msg->bundle_id;
        b.src = msg->source_id;
        b.dst = msg->destination_id;
        b.priority = msg->priority;
        b.t_gen = msg->t_gen;
        b.deadline_sec = msg->deadline_sec;
        b.size_bytes = msg->payload_size_bytes;

        bundle_store_[b.id] = {b, msg->payload_bytes};
        router_->on_bundle_generated(b, now);

        RCLCPP_INFO(this->get_logger(),
            "[ConCGR Ingest] Bundle #%lu (Prio: %u, Size: %u B, Queue: %zu, In-Blackout: %s)",
            b.id, b.priority, b.size_bytes, router_->get_queue().size(), in_blackout ? "YES" : "NO");
    }

    void on_step() {
        double now = this->now().seconds();
        double elapsed = now - start_time_;

        // Emulate realistic mountain occlusion blackout
        bool in_blackout = (elapsed >= blackout_start_sec_ && elapsed < (blackout_start_sec_ + blackout_duration_sec_));

        if (in_blackout) {
            if (!blackout_active_) {
                blackout_active_ = true;
                RCLCPP_WARN(this->get_logger(),
                    "*** ENTERING FORCED 20s MOUNTAIN OCCLUSION BLACKOUT (t = %.1f s) ***", elapsed);
                router_->on_contact_end(1, 0, now);
            }
            return; // No RF propagation during mountain occlusion
        } else {
            if (blackout_active_) {
                blackout_active_ = false;
                RCLCPP_INFO(this->get_logger(),
                    "*** LINE-OF-SIGHT RESTORED TO GROUND STATION (t = %.1f s) - DRAINING CUSTODY BUFFER ***", elapsed);
                router_->on_contact_start(1, 0, now, 6.0e6);
            } else if (!initial_contact_opened_) {
                initial_contact_opened_ = true;
                router_->on_contact_start(1, 0, now, 6.0e6);
            }
        }

        // Execute ConCGR-AoI minimax routing decisions
        auto decisions = router_->step(now);
        for (const auto& dec : decisions) {
            forward_bundle(dec.bundle_id, dec.next_hop_uav_id);
        }
    }

    void forward_bundle(uint64_t bundle_id, uint32_t next_hop) {
        auto it = bundle_store_.find(bundle_id);
        if (it == bundle_store_.end()) return;

        const auto& b = it->second.meta;
        const auto& payload = it->second.payload;
        uint32_t payload_size = payload.size();

        const uint32_t max_chunk_size = 1400;
        uint16_t total_chunks = (payload_size + max_chunk_size - 1) / max_chunk_size;
        if (total_chunks == 0) total_chunks = 1;

        sockaddr_in dest_addr{};
        dest_addr.sin_family = AF_INET;
        int target_port = (next_hop == 0) ? sink_port_ : (5500 + next_hop);
        dest_addr.sin_port = htons(target_port);
        inet_pton(AF_INET, sink_ip_.c_str(), &dest_addr.sin_addr);

        uint32_t remaining = payload_size;
        for (uint16_t c = 0; c < total_chunks; ++c) {
            uint32_t this_chunk = std::min(remaining, max_chunk_size);
            std::vector<uint8_t> packet(sizeof(DtnHeader) + this_chunk);

            DtnHeader hdr;
            hdr.bundle_id = b.id;
            hdr.source_id = b.src;
            hdr.destination_id = b.dst;
            hdr.priority = b.priority;
            hdr.t_gen = b.t_gen;
            hdr.deadline_sec = b.deadline_sec;
            hdr.payload_size = payload_size;
            hdr.chunk_idx = c;
            hdr.total_chunks = total_chunks;

            std::memcpy(packet.data(), &hdr, sizeof(hdr));
            if (this_chunk > 0) {
                std::memcpy(packet.data() + sizeof(hdr), payload.data() + (c * max_chunk_size), this_chunk);
            }

            sendto(sock_fd_, packet.data(), packet.size(), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));

            if (c + 1 < total_chunks) {
                usleep(200);
            }

            if (remaining > this_chunk) remaining -= this_chunk;
            else remaining = 0;
        }

        RCLCPP_INFO(this->get_logger(),
            "[DTN Forward] Sent Bundle #%lu (Prio: %u, Size: %u bytes in %u chunks, t_gen: %.2f) to Target Port %d",
            bundle_id, b.priority, payload_size, total_chunks, b.t_gen, target_port);
    }

    void poll_incoming_udp() {
        if (sock_fd_ < 0) return;

        uint8_t buffer[65536];
        sockaddr_in sender_addr{};
        socklen_t sender_len = sizeof(sender_addr);

        while (true) {
            ssize_t bytes_read = recvfrom(sock_fd_, buffer, sizeof(buffer), 0,
                                         (struct sockaddr*)&sender_addr, &sender_len);
            if (bytes_read <= 0) break;

            if (bytes_read == sizeof(uint64_t)) {
                // Custody ACK
                uint64_t ack_id = 0;
                std::memcpy(&ack_id, buffer, sizeof(ack_id));
                router_->on_custody_ack(ack_id);
                bundle_store_.erase(ack_id);
                RCLCPP_INFO(this->get_logger(),
                    "[Custody ACK] Bundle #%lu cleared from local UAV storage (Remaining Queue: %zu)",
                    ack_id, router_->get_queue().size());
            }
        }
    }

    int uav_id_{1};
    int sink_port_{5500};
    int local_port_{5501};
    std::string sink_ip_{"127.0.0.1"};
    int sock_fd_{-1};

    double blackout_start_sec_{5.0};
    double blackout_duration_sec_{20.0};
    double start_time_{0.0};
    bool blackout_active_{false};
    bool initial_contact_opened_{false};

    std::unique_ptr<concgr::ConCgrAoiRouter> router_;
    std::unordered_map<uint64_t, StoredBundle> bundle_store_;

    rclcpp::Subscription<concgr_msgs::msg::BundlePacket>::SharedPtr sub_sensor_;
    rclcpp::Publisher<concgr_msgs::msg::BundlePacket>::SharedPtr pub_direct_dds_;
    rclcpp::TimerBase::SharedPtr timer_step_;
    rclcpp::TimerBase::SharedPtr timer_recv_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ConcgrBridgeNode>());
    rclcpp::shutdown();
    return 0;
}
