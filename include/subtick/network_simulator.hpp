#pragma once

#include "subtick/packet.hpp"
#include "subtick/clock.hpp"
#include <deque>
#include <vector>
#include <random>
#include <algorithm>

namespace subtick {

struct SimulatedPacket {
    Packet packet;
    uint64_t deliver_at_us{0};
};

struct NetworkSimulatorConfig {
    float packet_loss_rate{0.0f};      // 0.0 to 1.0 (e.g. 0.05 = 5% loss)
    uint32_t latency_ms{0};            // Base one-way latency in milliseconds
    uint32_t jitter_ms{0};             // Latency variance in milliseconds
    float duplicate_rate{0.0f};        // Duplicate packet injection rate
    float reorder_rate{0.0f};          // Out-of-order delivery rate
};

class NetworkSimulator {
public:
    using Config = NetworkSimulatorConfig;

    NetworkSimulator()
        : NetworkSimulator(Config{}, 42) {}

    explicit NetworkSimulator(Config config, uint32_t seed = 42)
        : config_(config), rng_(seed), dist_01_(0.0f, 1.0f) {}

    void send(Packet packet) {
        ++total_sent_;

        // Packet loss check
        if (config_.packet_loss_rate > 0.0f && dist_01_(rng_) < config_.packet_loss_rate) {
            ++total_dropped_;
            return; // Dropped
        }

        uint64_t now_us = Clock::now_us();
        uint64_t delay_us = calculate_delay_us();
        uint64_t deliver_time = now_us + delay_us;

        // Reordering check: randomly advance or delay delivery
        if (config_.reorder_rate > 0.0f && dist_01_(rng_) < config_.reorder_rate) {
            deliver_time += (5'000 + (static_cast<uint64_t>(dist_01_(rng_) * 20'000)));
        }

        queue_.push_back({packet, deliver_time});

        // Duplicate check
        if (config_.duplicate_rate > 0.0f && dist_01_(rng_) < config_.duplicate_rate) {
            queue_.push_back({packet, deliver_time + 1'000}); // Arrives 1ms later
        }

        // Keep queue sorted by delivery time
        std::sort(queue_.begin(), queue_.end(), [](const SimulatedPacket& a, const SimulatedPacket& b) {
            return a.deliver_at_us < b.deliver_at_us;
        });
    }

    // Advance simulator time and pop packets ready for delivery
    std::vector<Packet> poll_ready_packets(uint64_t current_time_us = Clock::now_us()) {
        std::vector<Packet> ready;
        while (!queue_.empty() && queue_.front().deliver_at_us <= current_time_us) {
            ready.push_back(std::move(queue_.front().packet));
            queue_.pop_front();
            ++total_delivered_;
        }
        return ready;
    }

    void set_config(const Config& config) { config_ = config; }
    [[nodiscard]] const Config& config() const { return config_; }

    [[nodiscard]] size_t pending_count() const { return queue_.size(); }
    [[nodiscard]] uint64_t total_sent() const { return total_sent_; }
    [[nodiscard]] uint64_t total_dropped() const { return total_dropped_; }
    [[nodiscard]] uint64_t total_delivered() const { return total_delivered_; }

private:
    uint64_t calculate_delay_us() {
        int64_t base_us = static_cast<int64_t>(config_.latency_ms) * 1000;
        if (config_.jitter_ms > 0) {
            std::uniform_int_distribution<int64_t> jitter_dist(
                -static_cast<int64_t>(config_.jitter_ms) * 1000,
                 static_cast<int64_t>(config_.jitter_ms) * 1000
            );
            base_us += jitter_dist(rng_);
        }
        return static_cast<uint64_t>(std::max<int64_t>(0, base_us));
    }

    Config config_;
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_01_;
    std::deque<SimulatedPacket> queue_;

    uint64_t total_sent_{0};
    uint64_t total_dropped_{0};
    uint64_t total_delivered_{0};
};

} // namespace subtick
