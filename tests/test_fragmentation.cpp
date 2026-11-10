#include "subtick/fragmentation.hpp"
#include <iostream>
#include <cstdlib>
#include <numeric>
#include <thread>
#include <chrono>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_fragmentation_and_out_of_order_reassembly() {
    subtick::Fragmenter fragmenter(1000); // 1000 bytes max per fragment
    subtick::Reassembler reassembler;

    // Generate 3500 bytes of deterministic test payload
    std::vector<uint8_t> original_payload(3500);
    for (size_t i = 0; i < original_payload.size(); ++i) {
        original_payload[i] = static_cast<uint8_t>(i % 256);
    }

    uint16_t msg_id = 99;
    auto fragments = fragmenter.split(original_payload, msg_id);
    TEST_ASSERT(fragments.size() == 4); // 3500 / 1000 -> 4 fragments

    // Feed fragments to reassembler OUT OF ORDER (index 3, then 1, then 0, then 2)
    auto r3 = reassembler.ingest_fragment(fragments[3]);
    TEST_ASSERT(!r3.has_value());

    auto r1 = reassembler.ingest_fragment(fragments[1]);
    TEST_ASSERT(!r1.has_value());

    auto r0 = reassembler.ingest_fragment(fragments[0]);
    TEST_ASSERT(!r0.has_value());

    // Final missing fragment: should trigger complete reassembly!
    auto final_result = reassembler.ingest_fragment(fragments[2]);
    TEST_ASSERT(final_result.has_value());
    TEST_ASSERT(final_result->size() == original_payload.size());
    TEST_ASSERT(*final_result == original_payload);

    std::cout << "[PASS] test_fragmentation_and_out_of_order_reassembly" << std::endl;
}

void test_token_bucket_limiter() {
    // 1000 bytes/sec, max burst 2000 bytes
    subtick::TokenBucketLimiter limiter(1000.0, 2000.0);

    TEST_ASSERT(limiter.can_send(1500));
    TEST_ASSERT(limiter.consume(1500));

    // Only ~500 tokens remaining, cannot send 1000 immediately
    TEST_ASSERT(!limiter.consume(1000));

    // Sleep 600ms -> generates ~600 tokens -> now total is ~1100 tokens
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    TEST_ASSERT(limiter.consume(1000));

    std::cout << "[PASS] test_token_bucket_limiter" << std::endl;
}

int main() {
    std::cout << "--- Running Fragmentation & Rate Limiting Tests ---" << std::endl;
    test_fragmentation_and_out_of_order_reassembly();
    test_token_bucket_limiter();
    return 0;
}
