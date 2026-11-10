#pragma once

#include "subtick/config.hpp"
#include "subtick/subtick_queue.hpp"
#include <deque>
#include <vector>
#include <cmath>
#include <functional>

namespace subtick {

struct PlayerState {
    uint32_t tick{0};
    uint16_t command_number{0};
    Vector3 position;
    Vector3 velocity;
    bool on_ground{true};

    [[nodiscard]] float distance_to(const PlayerState& other) const {
        float dx = position.x - other.position.x;
        float dy = position.y - other.position.y;
        float dz = position.z - other.position.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
};

struct PlayerInput {
    uint16_t command_number{0};
    uint32_t tick{0};
    Vector3 movement; // Normalized [-1.0, 1.0] forward/side/up
    float delta_time{1.0f / 64.0f}; // Default 64-tick step
    bool jump{false};
};

struct PredictionEntry {
    PlayerInput input;
    PlayerState predicted_state;
};

// Client-side prediction and server reconciliation with rollback
class ClientPredictor {
public:
    using SimulationStep = std::function<PlayerState(const PlayerState&, const PlayerInput&)>;

    explicit ClientPredictor(float reconciliation_threshold = 0.05f)
        : error_threshold_(reconciliation_threshold) {
        // Default simple kinematic physics simulator
        physics_step_ = [](const PlayerState& current, const PlayerInput& in) {
            PlayerState next = current;
            next.tick = in.tick;
            next.command_number = in.command_number;

            float speed = 250.0f; // Movement speed
            next.velocity.x = in.movement.x * speed;
            next.velocity.y = in.movement.y * speed;

            if (in.jump && next.on_ground) {
                next.velocity.z = 300.0f; // Jump impulse
                next.on_ground = false;
            } else if (!next.on_ground) {
                next.velocity.z -= 800.0f * in.delta_time; // Gravity
            }

            next.position.x += next.velocity.x * in.delta_time;
            next.position.y += next.velocity.y * in.delta_time;
            next.position.z += next.velocity.z * in.delta_time;

            if (next.position.z <= 0.0f) {
                next.position.z = 0.0f;
                next.velocity.z = 0.0f;
                next.on_ground = true;
            }

            return next;
        };
    }

    void set_custom_physics_step(SimulationStep step) {
        physics_step_ = std::move(step);
    }

    // Predicts the next local player state based on immediate input
    PlayerState predict(const PlayerInput& input) {
        PlayerState next_state = physics_step_(current_state_, input);
        current_state_ = next_state;

        history_.push_back({input, next_state});
        while (history_.size() > 256) {
            history_.pop_front();
        }

        return current_state_;
    }

    // Reconciles local predicted history with authoritative server snapshot
    bool reconcile(const PlayerState& server_state) {
        // Find matching historical command
        auto it = std::find_if(history_.begin(), history_.end(),
            [&server_state](const PredictionEntry& entry) {
                return entry.input.command_number == server_state.command_number;
            });

        if (it == history_.end()) {
            return false; // Outdated or unknown snapshot
        }

        float error = it->predicted_state.distance_to(server_state);
        latest_error_ = error;

        // Discard all acknowledged history prior to and including this snapshot
        history_.erase(history_.begin(), it + 1);

        if (error > error_threshold_) {
            ++total_rollbacks_;
            // Rollback: snap to authoritative state
            current_state_ = server_state;

            // Re-simulate all remaining unacknowledged inputs
            for (auto& entry : history_) {
                entry.predicted_state = physics_step_(current_state_, entry.input);
                current_state_ = entry.predicted_state;
            }
            return true; // Rollback occurred
        }

        return false; // Prediction was accurate
    }

    [[nodiscard]] const PlayerState& current_state() const { return current_state_; }
    [[nodiscard]] size_t unacknowledged_count() const { return history_.size(); }
    [[nodiscard]] uint64_t total_rollbacks() const { return total_rollbacks_; }
    [[nodiscard]] float latest_error() const { return latest_error_; }

    void reset(const PlayerState& initial_state = {}) {
        current_state_ = initial_state;
        history_.clear();
        total_rollbacks_ = 0;
        latest_error_ = 0.0f;
    }

private:
    float error_threshold_{0.05f};
    PlayerState current_state_;
    std::deque<PredictionEntry> history_;
    SimulationStep physics_step_;

    uint64_t total_rollbacks_{0};
    float latest_error_{0.0f};
};

} // namespace subtick
