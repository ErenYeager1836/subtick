#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <compare>

namespace subtick {

using DurationMicros = std::chrono::microseconds;
using TimePoint = std::chrono::steady_clock::time_point;

class Clock {
public:
    static uint64_t now_us() {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
    }

    static uint64_t now_ms() {
        return now_us() / 1000;
    }

    static double now_sec() {
        return static_cast<double>(now_us()) / 1'000'000.0;
    }
};

// Represents exact game time with sub-tick precision (tick + microsecond offset)
struct SubTickTime {
    uint32_t tick{0};             // Discrete server tick (e.g. 64 ticks per second)
    float fraction{0.0f};         // Fraction of the tick when event occurred (0.0 to 1.0)
    uint64_t timestamp_us{0};     // Wall-clock microsecond timestamp

    [[nodiscard]] double total_ticks() const {
        return static_cast<double>(tick) + static_cast<double>(fraction);
    }

    bool operator==(const SubTickTime& other) const = default;
    std::partial_ordering operator<=>(const SubTickTime& other) const {
        if (tick != other.tick) {
            return (tick < other.tick) ? std::partial_ordering::less : std::partial_ordering::greater;
        }
        return fraction <=> other.fraction;
    }
};

// Synchronizes client game clock with server authoritative clock
class ClockSynchronizer {
public:
    void update(uint64_t client_send_time_us, uint64_t server_receive_time_us, uint64_t client_recv_time_us) {
        if (client_recv_time_us <= client_send_time_us) return;

        uint64_t rtt_us = client_recv_time_us - client_send_time_us;
        // Estimated one-way latency: RTT / 2
        int64_t calculated_offset_us = static_cast<int64_t>(server_receive_time_us) -
                                       static_cast<int64_t>(client_send_time_us + (rtt_us / 2));

        if (!initialized_) {
            offset_us_ = calculated_offset_us;
            smoothed_rtt_us_ = static_cast<double>(rtt_us);
            initialized_ = true;
        } else {
            // Exponential Moving Average (alpha = 0.1 for offset, 0.125 for RTT)
            offset_us_ = static_cast<int64_t>(offset_us_ * 0.9 + calculated_offset_us * 0.1);
            smoothed_rtt_us_ = smoothed_rtt_us_ * 0.875 + rtt_us * 0.125;
        }
    }

    [[nodiscard]] int64_t clock_offset_us() const { return offset_us_; }
    [[nodiscard]] double round_trip_time_ms() const { return smoothed_rtt_us_ / 1000.0; }
    [[nodiscard]] bool is_synchronized() const { return initialized_; }

    [[nodiscard]] uint64_t client_to_server_time(uint64_t client_time_us) const {
        return static_cast<uint64_t>(static_cast<int64_t>(client_time_us) + offset_us_);
    }

private:
    int64_t offset_us_{0};
    double smoothed_rtt_us_{0.0};
    bool initialized_{false};
};

} // namespace subtick
