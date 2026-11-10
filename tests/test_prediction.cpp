#include "subtick/prediction.hpp"
#include <iostream>
#include <cstdlib>
#include <cmath>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_client_prediction_and_reconciliation() {
    subtick::ClientPredictor predictor(0.01f);

    // Initial state at position (0, 0, 0)
    subtick::PlayerState initial;
    initial.position = {0.0f, 0.0f, 0.0f};
    predictor.reset(initial);

    // Client executes 3 consecutive forward movement commands
    subtick::PlayerInput in1{1, 10, {1.0f, 0.0f, 0.0f}, 1.0f / 64.0f, false};
    subtick::PlayerInput in2{2, 11, {1.0f, 0.0f, 0.0f}, 1.0f / 64.0f, false};
    subtick::PlayerInput in3{3, 12, {1.0f, 0.0f, 0.0f}, 1.0f / 64.0f, false};

    auto p1 = predictor.predict(in1);
    auto p2 = predictor.predict(in2);
    auto p3 = predictor.predict(in3);

    TEST_ASSERT(predictor.unacknowledged_count() == 3);
    TEST_ASSERT(p3.position.x > p2.position.x && p2.position.x > p1.position.x);

    // 1. Server acknowledges command 1 with EXACT match
    bool rollback = predictor.reconcile(p1);
    TEST_ASSERT(!rollback); // No error, prediction was accurate!
    TEST_ASSERT(predictor.unacknowledged_count() == 2);

    // 2. Server acknowledges command 2, but with a desync! (e.g. server reports obstacle collision at X=2.0)
    subtick::PlayerState desynced_server_state = p2;
    desynced_server_state.position.x = 2.0f; // Significant deviation

    rollback = predictor.reconcile(desynced_server_state);
    TEST_ASSERT(rollback); // Rollback MUST trigger!
    TEST_ASSERT(predictor.total_rollbacks() == 1);
    TEST_ASSERT(predictor.unacknowledged_count() == 1);

    // The current state must be re-simulated from the new baseline (X=2.0) + command 3
    float expected_x = 2.0f + (250.0f * (1.0f / 64.0f));
    TEST_ASSERT(std::abs(predictor.current_state().position.x - expected_x) < 0.01f);

    std::cout << "[PASS] test_client_prediction_and_reconciliation" << std::endl;
}

int main() {
    std::cout << "--- Running Client Prediction Tests ---" << std::endl;
    test_client_prediction_and_reconciliation();
    return 0;
}
