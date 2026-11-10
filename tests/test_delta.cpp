#include "subtick/delta.hpp"
#include <iostream>
#include <cstdlib>
#include <cmath>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_delta_compression_efficiency() {
    subtick::EntityState baseline;
    baseline.entity_id = 1;
    baseline.tick = 100;
    baseline.position = {100.0f, 200.0f, 50.0f};
    baseline.velocity = {0.0f, 0.0f, 0.0f};
    baseline.rotation = {0.0f, 45.0f, 0.0f};
    baseline.health = 100;

    // Full baseline state serialization
    subtick::BitWriter full_writer;
    baseline.write_delta(full_writer, nullptr);
    size_t full_bytes = full_writer.byte_size();

    // Next tick: only position X moved slightly, health and rotation unchanged
    subtick::EntityState current = baseline;
    current.tick = 101;
    current.position.x = 105.0f;

    subtick::BitWriter delta_writer;
    current.write_delta(delta_writer, &baseline);
    size_t delta_bytes = delta_writer.byte_size();

    // Delta serialization should be dramatically smaller than full baseline!
    TEST_ASSERT(delta_bytes < full_bytes);
    std::cout << "Full state: " << full_bytes << " bytes vs Delta state: " << delta_bytes << " bytes" << std::endl;

    // Decode delta against baseline
    subtick::BitReader delta_reader(delta_writer.bytes());
    subtick::EntityState decoded = subtick::EntityState::read_delta(delta_reader, &baseline);

    TEST_ASSERT(decoded.entity_id == 1);
    TEST_ASSERT(decoded.tick == 101);
    TEST_ASSERT(std::abs(decoded.position.x - 105.0f) < 0.1f);
    TEST_ASSERT(std::abs(decoded.position.y - 200.0f) < 0.1f);
    TEST_ASSERT(decoded.health == 100);

    std::cout << "[PASS] test_delta_compression_efficiency" << std::endl;
}

int main() {
    std::cout << "--- Running Delta Compression Tests ---" << std::endl;
    test_delta_compression_efficiency();
    return 0;
}
