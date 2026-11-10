#pragma once

#include "subtick/config.hpp"
#include "subtick/clock.hpp"
#include "subtick/bitstream.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <cstdint>

namespace subtick {

enum class ActionType : uint8_t {
    NONE = 0,
    MOVE = 1,
    ATTACK = 2,
    SECONDARY_ATTACK = 3,
    JUMP = 4,
    CROUCH = 5,
    RELOAD = 6,
    CUSTOM = 7
};

struct Vector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    bool operator==(const Vector3& other) const = default;
};

struct SubTickInputEvent {
    SubTickTime time;
    ActionType action{ActionType::NONE};
    Vector3 view_angles; // Pitch, Yaw, Roll
    Vector3 movement;    // Forward, Side, Up
    uint32_t buttons{0};
    uint16_t command_number{0};

    void serialize(BitWriter& writer) const {
        writer.write_u32(time.tick);
        // Quantize sub-tick fraction into 12 bits (resolution: ~0.000244 tick)
        writer.write_quantized_float(time.fraction, 0.0f, 1.0f, 12);
        writer.write_bits(static_cast<uint32_t>(action), 4);
        writer.write_u32(buttons);
        writer.write_u16(command_number);

        // Quantize view angles: pitch [-90, +90], yaw [-180, +180] in 16 bits each
        writer.write_quantized_float(view_angles.x, -90.0f, 90.0f, 16);
        writer.write_quantized_float(view_angles.y, -180.0f, 180.0f, 16);
        writer.write_quantized_float(view_angles.z, -180.0f, 180.0f, 16);

        // Movement components: clamp to [-1.0, 1.0] in 8 bits each
        writer.write_quantized_float(movement.x, -1.0f, 1.0f, 8);
        writer.write_quantized_float(movement.y, -1.0f, 1.0f, 8);
        writer.write_quantized_float(movement.z, -1.0f, 1.0f, 8);
    }

    static SubTickInputEvent deserialize(BitReader& reader) {
        SubTickInputEvent ev;
        ev.time.tick = reader.read_u32();
        ev.time.fraction = reader.read_quantized_float(0.0f, 1.0f, 12);
        ev.action = static_cast<ActionType>(reader.read_bits(4));
        ev.buttons = reader.read_u32();
        ev.command_number = reader.read_u16();

        ev.view_angles.x = reader.read_quantized_float(-90.0f, 90.0f, 16);
        ev.view_angles.y = reader.read_quantized_float(-180.0f, 180.0f, 16);
        ev.view_angles.z = reader.read_quantized_float(-180.0f, 180.0f, 16);

        ev.movement.x = reader.read_quantized_float(-1.0f, 1.0f, 8);
        ev.movement.y = reader.read_quantized_float(-1.0f, 1.0f, 8);
        ev.movement.z = reader.read_quantized_float(-1.0f, 1.0f, 8);

        return ev;
    }
};

class SubTickQueue {
public:
    void push(SubTickInputEvent event) {
        events_.push_back(std::move(event));
        // Sort chronologically by (tick, fraction)
        std::sort(events_.begin(), events_.end(), [](const SubTickInputEvent& a, const SubTickInputEvent& b) {
            return a.time < b.time;
        });
    }

    // Extract all events scheduled for a specific server tick
    std::vector<SubTickInputEvent> pop_events_for_tick(uint32_t tick) {
        std::vector<SubTickInputEvent> result;
        auto it = events_.begin();
        while (it != events_.end()) {
            if (it->time.tick == tick) {
                result.push_back(*it);
                it = events_.erase(it);
            } else if (it->time.tick < tick) {
                // Outdated event from past tick; collect to avoid loss
                result.push_back(*it);
                it = events_.erase(it);
            } else {
                ++it;
            }
        }
        return result;
    }

    [[nodiscard]] size_t size() const { return events_.size(); }
    [[nodiscard]] bool empty() const { return events_.empty(); }
    void clear() { events_.clear(); }

private:
    std::deque<SubTickInputEvent> events_;
};

} // namespace subtick
