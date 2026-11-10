#pragma once

#include "subtick/config.hpp"
#include "subtick/clock.hpp"
#include "subtick/subtick_queue.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <cmath>
#include <limits>

namespace subtick {

struct Ray {
    Vector3 origin;
    Vector3 direction; // Normalized direction vector
};

struct AABB {
    Vector3 min;
    Vector3 max;

    [[nodiscard]] bool contains(const Vector3& p) const {
        return (p.x >= min.x && p.x <= max.x) &&
               (p.y >= min.y && p.y <= max.y) &&
               (p.z >= min.z && p.z <= max.z);
    }

    // Fast Slab Method 3D Ray-AABB intersection test
    [[nodiscard]] bool intersects(const Ray& ray, float max_dist, float& hit_dist) const {
        float tmin = 0.0f;
        float tmax = max_dist;

        // X slab
        if (std::abs(ray.direction.x) > 1e-6f) {
            float inv_d = 1.0f / ray.direction.x;
            float t1 = (min.x - ray.origin.x) * inv_d;
            float t2 = (max.x - ray.origin.x) * inv_d;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        } else if (ray.origin.x < min.x || ray.origin.x > max.x) {
            return false;
        }

        // Y slab
        if (std::abs(ray.direction.y) > 1e-6f) {
            float inv_d = 1.0f / ray.direction.y;
            float t1 = (min.y - ray.origin.y) * inv_d;
            float t2 = (max.y - ray.origin.y) * inv_d;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        } else if (ray.origin.y < min.y || ray.origin.y > max.y) {
            return false;
        }

        // Z slab
        if (std::abs(ray.direction.z) > 1e-6f) {
            float inv_d = 1.0f / ray.direction.z;
            float t1 = (min.z - ray.origin.z) * inv_d;
            float t2 = (max.z - ray.origin.z) * inv_d;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        } else if (ray.origin.z < min.z || ray.origin.z > max.z) {
            return false;
        }

        hit_dist = tmin;
        return true;
    }

    [[nodiscard]] static AABB lerp(const AABB& a, const AABB& b, float t) {
        AABB res;
        res.min.x = a.min.x + t * (b.min.x - a.min.x);
        res.min.y = a.min.y + t * (b.min.y - a.min.y);
        res.min.z = a.min.z + t * (b.min.z - a.min.z);
        res.max.x = a.max.x + t * (b.max.x - a.max.x);
        res.max.y = a.max.y + t * (b.max.y - a.max.y);
        res.max.z = a.max.z + t * (b.max.z - a.max.z);
        return res;
    }
};

struct HitboxHistoryEntry {
    SubTickTime time;
    AABB box;
};

struct HitResult {
    bool hit{false};
    uint32_t target_entity_id{0};
    float distance{0.0f};
    Vector3 hit_point;
};

// Maintains past entity bounding boxes to rewind time during hit validation
class LagCompensator {
public:
    explicit LagCompensator(size_t max_history_per_entity = 128)
        : max_history_(max_history_per_entity) {}

    void record_entity_hitbox(uint32_t entity_id, SubTickTime time, const AABB& box) {
        auto& history = entity_histories_[entity_id];
        history.push_back({time, box});
        while (history.size() > max_history_) {
            history.pop_front();
        }
    }

    // Interpolates an entity's hitbox at any exact historical sub-tick time
    [[nodiscard]] std::optional<AABB> get_interpolated_hitbox(uint32_t entity_id, const SubTickTime& target_time) const {
        auto it = entity_histories_.find(entity_id);
        if (it == entity_histories_.end() || it->second.empty()) {
            return std::nullopt;
        }

        const auto& history = it->second;
        if (target_time <= history.front().time) {
            return history.front().box;
        }
        if (target_time >= history.back().time) {
            return history.back().box;
        }

        // Find the bounding history pair
        for (size_t i = 0; i < history.size() - 1; ++i) {
            if (history[i].time <= target_time && target_time <= history[i + 1].time) {
                double t0 = history[i].time.total_ticks();
                double t1 = history[i + 1].time.total_ticks();
                double target_t = target_time.total_ticks();
                float alpha = (t1 > t0) ? static_cast<float>((target_t - t0) / (t1 - t0)) : 0.0f;
                return AABB::lerp(history[i].box, history[i + 1].box, alpha);
            }
        }

        return history.back().box;
    }

    // Casts a shot ray against all entities rewound to the shooter's sub-tick time
    [[nodiscard]] HitResult verify_shot(uint32_t shooter_id, const SubTickTime& shot_time, const Ray& ray, float max_distance = 10000.0f) const {
        HitResult best_hit;
        best_hit.hit = false;
        best_hit.distance = max_distance;

        for (const auto& [entity_id, history] : entity_histories_) {
            if (entity_id == shooter_id) continue; // Don't hit yourself

            auto opt_box = get_interpolated_hitbox(entity_id, shot_time);
            if (!opt_box) continue;

            float hit_dist = 0.0f;
            if (opt_box->intersects(ray, best_hit.distance, hit_dist)) {
                if (hit_dist < best_hit.distance) {
                    best_hit.hit = true;
                    best_hit.target_entity_id = entity_id;
                    best_hit.distance = hit_dist;
                    best_hit.hit_point.x = ray.origin.x + ray.direction.x * hit_dist;
                    best_hit.hit_point.y = ray.origin.y + ray.direction.y * hit_dist;
                    best_hit.hit_point.z = ray.origin.z + ray.direction.z * hit_dist;
                }
            }
        }

        return best_hit;
    }

    void clear() {
        entity_histories_.clear();
    }

private:
    size_t max_history_{128};
    std::unordered_map<uint32_t, std::deque<HitboxHistoryEntry>> entity_histories_;
};

} // namespace subtick
