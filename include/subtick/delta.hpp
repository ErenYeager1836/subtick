#pragma once

#include "subtick/config.hpp"
#include "subtick/bitstream.hpp"
#include "subtick/subtick_queue.hpp"
#include <cstdint>
#include <vector>
#include <cmath>

namespace subtick {

enum EntityDeltaFlags : uint16_t {
    DELTA_NONE         = 0x0000,
    DELTA_POSITION_X   = 0x0001,
    DELTA_POSITION_Y   = 0x0002,
    DELTA_POSITION_Z   = 0x0004,
    DELTA_VELOCITY     = 0x0008,
    DELTA_ROTATION     = 0x0010,
    DELTA_HEALTH       = 0x0020,
    DELTA_FLAGS        = 0x0040,
    DELTA_ALL          = 0x007F
};

struct EntityState {
    uint32_t entity_id{0};
    uint32_t tick{0};
    Vector3 position;
    Vector3 velocity;
    Vector3 rotation;
    uint8_t health{100};
    uint8_t flags{0};

    [[nodiscard]] uint16_t compute_delta_mask(const EntityState& baseline) const {
        uint16_t mask = DELTA_NONE;
        if (std::abs(position.x - baseline.position.x) > 0.001f) mask |= DELTA_POSITION_X;
        if (std::abs(position.y - baseline.position.y) > 0.001f) mask |= DELTA_POSITION_Y;
        if (std::abs(position.z - baseline.position.z) > 0.001f) mask |= DELTA_POSITION_Z;
        if (std::abs(velocity.x - baseline.velocity.x) > 0.001f ||
            std::abs(velocity.y - baseline.velocity.y) > 0.001f ||
            std::abs(velocity.z - baseline.velocity.z) > 0.001f) mask |= DELTA_VELOCITY;
        if (std::abs(rotation.x - baseline.rotation.x) > 0.01f ||
            std::abs(rotation.y - baseline.rotation.y) > 0.01f ||
            std::abs(rotation.z - baseline.rotation.z) > 0.01f) mask |= DELTA_ROTATION;
        if (health != baseline.health) mask |= DELTA_HEALTH;
        if (flags != baseline.flags) mask |= DELTA_FLAGS;
        return mask;
    }

    void write_delta(BitWriter& writer, const EntityState* baseline = nullptr) const {
        writer.write_u32(entity_id);
        writer.write_u32(tick);

        if (!baseline) {
            // Full state baseline snapshot
            writer.write_u16(DELTA_ALL);
            writer.write_quantized_float(position.x, -10000.0f, 10000.0f, 20);
            writer.write_quantized_float(position.y, -10000.0f, 10000.0f, 20);
            writer.write_quantized_float(position.z, -10000.0f, 10000.0f, 20);
            writer.write_quantized_float(velocity.x, -2000.0f, 2000.0f, 14);
            writer.write_quantized_float(velocity.y, -2000.0f, 2000.0f, 14);
            writer.write_quantized_float(velocity.z, -2000.0f, 2000.0f, 14);
            writer.write_quantized_float(rotation.x, -90.0f, 90.0f, 12);
            writer.write_quantized_float(rotation.y, -180.0f, 180.0f, 12);
            writer.write_quantized_float(rotation.z, -180.0f, 180.0f, 12);
            writer.write_u8(health);
            writer.write_u8(flags);
            return;
        }

        uint16_t mask = compute_delta_mask(*baseline);
        writer.write_u16(mask);

        if (mask & DELTA_POSITION_X) writer.write_quantized_float(position.x, -10000.0f, 10000.0f, 20);
        if (mask & DELTA_POSITION_Y) writer.write_quantized_float(position.y, -10000.0f, 10000.0f, 20);
        if (mask & DELTA_POSITION_Z) writer.write_quantized_float(position.z, -10000.0f, 10000.0f, 20);
        if (mask & DELTA_VELOCITY) {
            writer.write_quantized_float(velocity.x, -2000.0f, 2000.0f, 14);
            writer.write_quantized_float(velocity.y, -2000.0f, 2000.0f, 14);
            writer.write_quantized_float(velocity.z, -2000.0f, 2000.0f, 14);
        }
        if (mask & DELTA_ROTATION) {
            writer.write_quantized_float(rotation.x, -90.0f, 90.0f, 12);
            writer.write_quantized_float(rotation.y, -180.0f, 180.0f, 12);
            writer.write_quantized_float(rotation.z, -180.0f, 180.0f, 12);
        }
        if (mask & DELTA_HEALTH) writer.write_u8(health);
        if (mask & DELTA_FLAGS) writer.write_u8(flags);
    }

    static EntityState read_delta(BitReader& reader, const EntityState* baseline = nullptr) {
        EntityState state = baseline ? *baseline : EntityState{};
        state.entity_id = reader.read_u32();
        state.tick = reader.read_u32();
        uint16_t mask = reader.read_u16();

        if (mask & DELTA_POSITION_X) state.position.x = reader.read_quantized_float(-10000.0f, 10000.0f, 20);
        if (mask & DELTA_POSITION_Y) state.position.y = reader.read_quantized_float(-10000.0f, 10000.0f, 20);
        if (mask & DELTA_POSITION_Z) state.position.z = reader.read_quantized_float(-10000.0f, 10000.0f, 20);
        if (mask & DELTA_VELOCITY) {
            state.velocity.x = reader.read_quantized_float(-2000.0f, 2000.0f, 14);
            state.velocity.y = reader.read_quantized_float(-2000.0f, 2000.0f, 14);
            state.velocity.z = reader.read_quantized_float(-2000.0f, 2000.0f, 14);
        }
        if (mask & DELTA_ROTATION) {
            state.rotation.x = reader.read_quantized_float(-90.0f, 90.0f, 12);
            state.rotation.y = reader.read_quantized_float(-180.0f, 180.0f, 12);
            state.rotation.z = reader.read_quantized_float(-180.0f, 180.0f, 12);
        }
        if (mask & DELTA_HEALTH) state.health = reader.read_u8();
        if (mask & DELTA_FLAGS) state.flags = reader.read_u8();

        return state;
    }
};

} // namespace subtick
