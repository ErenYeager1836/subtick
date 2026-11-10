#include "subtick/subtick_queue.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_subtick_event_sorting() {
    subtick::SubTickQueue queue;

    // Push events out-of-order within tick 100
    subtick::SubTickInputEvent e1;
    e1.time = {100, 0.85f, 1000};
    e1.action = subtick::ActionType::JUMP;

    subtick::SubTickInputEvent e2;
    e2.time = {100, 0.12f, 1001};
    e2.action = subtick::ActionType::ATTACK;

    subtick::SubTickInputEvent e3;
    e3.time = {100, 0.45f, 1002};
    e3.action = subtick::ActionType::MOVE;

    queue.push(e1);
    queue.push(e2);
    queue.push(e3);

    auto events = queue.pop_events_for_tick(100);
    TEST_ASSERT(events.size() == 3);

    // Should be sorted chronologically: 0.12 -> 0.45 -> 0.85
    TEST_ASSERT(events[0].action == subtick::ActionType::ATTACK);
    TEST_ASSERT(events[1].action == subtick::ActionType::MOVE);
    TEST_ASSERT(events[2].action == subtick::ActionType::JUMP);

    std::cout << "[PASS] test_subtick_event_sorting" << std::endl;
}

void test_subtick_serialization() {
    subtick::SubTickInputEvent original;
    original.time = {64, 0.354f, 5555};
    original.action = subtick::ActionType::ATTACK;
    original.buttons = 1;
    original.command_number = 42;
    original.view_angles = {12.5f, -85.2f, 0.0f};
    original.movement = {1.0f, 0.0f, 0.0f};

    subtick::BitWriter writer;
    original.serialize(writer);

    subtick::BitReader reader(writer.bytes());
    auto decoded = subtick::SubTickInputEvent::deserialize(reader);

    TEST_ASSERT(decoded.time.tick == original.time.tick);
    TEST_ASSERT(std::abs(decoded.time.fraction - original.time.fraction) < 0.001f);
    TEST_ASSERT(decoded.action == original.action);
    TEST_ASSERT(decoded.buttons == original.buttons);
    TEST_ASSERT(decoded.command_number == original.command_number);
    TEST_ASSERT(std::abs(decoded.view_angles.x - original.view_angles.x) < 0.05f);
    TEST_ASSERT(std::abs(decoded.view_angles.y - original.view_angles.y) < 0.05f);

    std::cout << "[PASS] test_subtick_serialization" << std::endl;
}

int main() {
    std::cout << "--- Running Sub-Tick Queue Tests ---" << std::endl;
    test_subtick_event_sorting();
    test_subtick_serialization();
    return 0;
}
