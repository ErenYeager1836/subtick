#include "subtick/lag_compensation.hpp"
#include <iostream>
#include <cstdlib>
#include <cmath>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_ray_aabb_intersection() {
    subtick::AABB box;
    box.min = {10.0f, -5.0f, -5.0f};
    box.max = {20.0f, 5.0f, 5.0f};

    // Ray aimed straight into center of box
    subtick::Ray direct_hit;
    direct_hit.origin = {0.0f, 0.0f, 0.0f};
    direct_hit.direction = {1.0f, 0.0f, 0.0f};

    float hit_dist = 0.0f;
    bool hit = box.intersects(direct_hit, 1000.0f, hit_dist);
    TEST_ASSERT(hit);
    TEST_ASSERT(std::abs(hit_dist - 10.0f) < 0.001f);

    // Ray pointed away from box
    subtick::Ray miss_ray;
    miss_ray.origin = {0.0f, 0.0f, 0.0f};
    miss_ray.direction = {-1.0f, 0.0f, 0.0f};
    hit = box.intersects(miss_ray, 1000.0f, hit_dist);
    TEST_ASSERT(!hit);

    std::cout << "[PASS] test_ray_aabb_intersection" << std::endl;
}

void test_lag_compensated_rewind_shot() {
    subtick::LagCompensator server_lag_comp;

    uint32_t target_player_id = 2;
    uint32_t shooter_player_id = 1;

    // Enemy target moves across the map from X=10 to X=50
    // Tick 100: Enemy is at X=10
    subtick::AABB b100{{10.0f, -2.0f, 0.0f}, {14.0f, 2.0f, 10.0f}};
    server_lag_comp.record_entity_hitbox(target_player_id, {100, 0.0f, 100000}, b100);

    // Tick 101: Enemy is at X=20
    subtick::AABB b101{{20.0f, -2.0f, 0.0f}, {24.0f, 2.0f, 10.0f}};
    server_lag_comp.record_entity_hitbox(target_player_id, {101, 0.0f, 101000}, b101);

    // Tick 102: Enemy is at X=30
    subtick::AABB b102{{30.0f, -2.0f, 0.0f}, {34.0f, 2.0f, 10.0f}};
    server_lag_comp.record_entity_hitbox(target_player_id, {102, 0.0f, 102000}, b102);

    // At present time (Tick 102), enemy is at X=30.
    // But shooter (with latency) clicked fire at sub-tick 100.5 (when enemy was interpolated at X=15)!
    subtick::SubTickTime shot_time{100, 0.5f, 100500};

    // Shooter fires ray at X=15
    subtick::Ray shot_ray{{15.0f, -100.0f, 5.0f}, {0.0f, 1.0f, 0.0f}};

    // Verify shot using server lag compensation
    subtick::HitResult result = server_lag_comp.verify_shot(shooter_player_id, shot_time, shot_ray);

    // The shot MUST HIT because lag compensation rewound target to X=15!
    TEST_ASSERT(result.hit);
    TEST_ASSERT(result.target_entity_id == target_player_id);
    TEST_ASSERT(std::abs(result.hit_point.x - 15.0f) < 0.01f);

    std::cout << "[PASS] test_lag_compensated_rewind_shot" << std::endl;
}

int main() {
    std::cout << "--- Running Lag Compensation Tests ---" << std::endl;
    test_ray_aabb_intersection();
    test_lag_compensated_rewind_shot();
    return 0;
}
