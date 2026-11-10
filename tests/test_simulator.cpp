#include "subtick/network_simulator.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_network_simulator_loss_and_latency() {
    subtick::NetworkSimulator::Config config;
    config.packet_loss_rate = 0.5f; // 50% packet loss
    config.latency_ms = 10;         // 10ms latency
    config.jitter_ms = 0;

    subtick::NetworkSimulator sim(config, 12345);

    // Send 100 packets
    for (int i = 0; i < 100; ++i) {
        subtick::Packet pkt;
        pkt.header.sequence = static_cast<uint16_t>(i);
        sim.send(pkt);
    }

    TEST_ASSERT(sim.total_sent() == 100);
    // With 50% loss over 100 packets, roughly 40-60 dropped
    TEST_ASSERT(sim.total_dropped() > 25 && sim.total_dropped() < 75);

    // Immediate poll: should be 0 because 10ms latency has not elapsed
    auto immediate = sim.poll_ready_packets(subtick::Clock::now_us());
    TEST_ASSERT(immediate.empty());

    // Wait 15ms
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    auto ready = sim.poll_ready_packets(subtick::Clock::now_us());
    TEST_ASSERT(!ready.empty());
    TEST_ASSERT(ready.size() == (sim.total_sent() - sim.total_dropped()));

    std::cout << "[PASS] test_network_simulator_loss_and_latency" << std::endl;
}

int main() {
    std::cout << "--- Running Network Simulator Tests ---" << std::endl;
    test_network_simulator_loss_and_latency();
    return 0;
}
