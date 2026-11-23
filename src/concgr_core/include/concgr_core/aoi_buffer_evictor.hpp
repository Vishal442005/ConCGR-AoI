#pragma once

#include "concgr_core/types.hpp"
#include <vector>
#include <algorithm>
#include <optional>

namespace concgr {

class AoiBufferEvictor {
public:
    struct Config {
        size_t max_buffer_bytes{8 * 1024 * 1024}; // 8 MB default onboard buffer limit
        double epsilon{1e-3};                     // Avoid division by zero
    };

    AoiBufferEvictor();
    explicit AoiBufferEvictor(const Config& config);

    // Calculate bundle eviction utility U(b, t) (Eq. 12)
    double compute_utility(const Bundle& bundle, double current_time) const;

    // Try to store a bundle; if buffer is full, evict lower-utility bundles
    // Returns list of evicted bundle IDs
    std::vector<uint64_t> admit_bundle(std::vector<Bundle>& queue, const Bundle& incoming, double current_time);

    void remove_delivered_or_expired(std::vector<Bundle>& queue, uint64_t bundle_id);

    size_t get_current_bytes() const { return current_buffer_bytes_; }
    size_t get_max_bytes() const { return config_.max_buffer_bytes; }

private:
    Config config_;
    size_t current_buffer_bytes_{0};
};

inline AoiBufferEvictor::AoiBufferEvictor()
    : AoiBufferEvictor(Config{})
{
}

inline AoiBufferEvictor::AoiBufferEvictor(const Config& config)
    : config_(config), current_buffer_bytes_(0)
{
}

inline double AoiBufferEvictor::compute_utility(const Bundle& bundle, double current_time) const {
    double age = std::max(0.0, current_time - bundle.t_gen);
    double size_factor = std::max(1.0, static_cast<double>(bundle.size_bytes));
    double priority_weight = static_cast<double>(bundle.priority);
    return priority_weight / ((age + config_.epsilon) * size_factor);
}

inline std::vector<uint64_t> AoiBufferEvictor::admit_bundle(
    std::vector<Bundle>& queue, const Bundle& incoming, double current_time)
{
    std::vector<uint64_t> evicted_ids;
    size_t incoming_size = incoming.size_bytes;

    if (incoming_size > config_.max_buffer_bytes) {
        evicted_ids.push_back(incoming.id);
        return evicted_ids;
    }

    while (current_buffer_bytes_ + incoming_size > config_.max_buffer_bytes && !queue.empty()) {
        auto lowest_it = std::min_element(queue.begin(), queue.end(),
            [this, current_time](const Bundle& a, const Bundle& b) {
                return compute_utility(a, current_time) < compute_utility(b, current_time);
            });

        double lowest_util = compute_utility(*lowest_it, current_time);
        double incoming_util = compute_utility(incoming, current_time);

        if (incoming_util <= lowest_util) {
            evicted_ids.push_back(incoming.id);
            return evicted_ids;
        }

        evicted_ids.push_back(lowest_it->id);
        current_buffer_bytes_ -= lowest_it->size_bytes;
        queue.erase(lowest_it);
    }

    queue.push_back(incoming);
    current_buffer_bytes_ += incoming_size;
    return evicted_ids;
}

inline void AoiBufferEvictor::remove_delivered_or_expired(std::vector<Bundle>& queue, uint64_t bundle_id) {
    auto it = std::find_if(queue.begin(), queue.end(),
        [bundle_id](const Bundle& b) { return b.id == bundle_id; });
    if (it != queue.end()) {
        current_buffer_bytes_ -= it->size_bytes;
        queue.erase(it);
    }
}

} // namespace concgr
