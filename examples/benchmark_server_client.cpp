#include "subtick/subtick.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>

struct ConnectedClient {
    uint32_t client_id{0};
    subtick::ReliabilitySystem reliability;
    subtick::NetworkSimulator to_server;
    subtick::NetworkSimulator to_client;
    subtick::SubTickQueue input_queue;
    subtick::EntityState last_known_state;
};

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "       subtick : High-Performance Game Netcode Engine     " << std::endl;
    std::cout << "  Multi-Client Sub-Tick Simulation over Lossy UDP Network " << std::endl;
    std::cout << "==========================================================" << std::endl;

    constexpr size_t NUM_CLIENTS = 8;
    constexpr uint32_t TOTAL_TICKS = 128; // ~2 seconds of 64-tick simulation

    subtick::NetworkSimulator::Config net_cfg;
    net_cfg.packet_loss_rate = 0.05f; // 5% packet loss
    net_cfg.latency_ms = 25;          // 25ms one-way ping (50ms RTT)
    net_cfg.jitter_ms = 5;            // +/- 5ms jitter

    std::vector<ConnectedClient> clients(NUM_CLIENTS);
    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        clients[i].client_id = static_cast<uint32_t>(i + 1);
        clients[i].to_server.set_config(net_cfg);
        clients[i].to_client.set_config(net_cfg);
        clients[i].last_known_state.entity_id = static_cast<uint32_t>(i + 1);
        clients[i].last_known_state.position = {0.0f, 0.0f, 0.0f};
    }

    uint64_t total_inputs_generated = 0;
    uint64_t total_inputs_processed = 0;
    size_t full_bytes_total = 0;
    size_t delta_bytes_total = 0;

    std::cout << "[*] Starting simulation with " << NUM_CLIENTS << " clients..." << std::endl;
    std::cout << "[*] Network Condition: 5% packet loss, 50ms RTT, +/-5ms jitter" << std::endl;

    for (uint32_t current_tick = 1; current_tick <= TOTAL_TICKS; ++current_tick) {
        // --- 1. Clients generate sub-tick inputs ---
        for (auto& client : clients) {
            subtick::SubTickInputEvent input;
            input.time.tick = current_tick;
            input.time.fraction = 0.42f; // Triggered at tick + 0.42 fraction
            input.action = (current_tick % 10 == 0) ? subtick::ActionType::ATTACK : subtick::ActionType::MOVE;
            input.movement = {1.0f, 0.0f, 0.0f};
            input.command_number = static_cast<uint16_t>(current_tick);

            // Client serializes input into packet
            subtick::Packet pkt;
            pkt.header = client.reliability.prepare_outgoing_header();
            subtick::BitWriter writer;
            input.serialize(writer);
            pkt.payload.assign(writer.bytes().begin(), writer.bytes().end());

            client.to_server.send(pkt);
            ++total_inputs_generated;
        }

        // --- 2. Server polls incoming packets ---
        for (auto& client : clients) {
            auto server_incoming = client.to_server.poll_ready_packets(subtick::Clock::now_us());
            for (const auto& pkt : server_incoming) {
                client.reliability.process_incoming_header(pkt.header);

                if (!pkt.payload.empty()) {
                    subtick::BitReader reader(pkt.payload);
                    auto event = subtick::SubTickInputEvent::deserialize(reader);
                    client.input_queue.push(event);
                }
            }

            // Server processes queued sub-tick inputs for current tick
            auto tick_events = client.input_queue.pop_events_for_tick(current_tick);
            total_inputs_processed += tick_events.size();

            // Server updates client entity state
            subtick::EntityState new_state = client.last_known_state;
            new_state.tick = current_tick;
            new_state.position.x += 1.5f; // Player moved

            // Server serializes state using delta compression
            subtick::BitWriter delta_writer;
            new_state.write_delta(delta_writer, &client.last_known_state);

            subtick::BitWriter full_writer;
            new_state.write_delta(full_writer, nullptr);

            full_bytes_total += full_writer.byte_size();
            delta_bytes_total += delta_writer.byte_size();

            client.last_known_state = new_state;

            // Server sends state update back to client
            subtick::Packet server_pkt;
            server_pkt.header = client.reliability.prepare_outgoing_header();
            server_pkt.payload.assign(delta_writer.bytes().begin(), delta_writer.bytes().end());
            client.to_client.send(server_pkt);
        }

        // --- 3. Clients poll server snapshots and acknowledge ---
        for (auto& client : clients) {
            auto client_incoming = client.to_client.poll_ready_packets(subtick::Clock::now_us());
            for (const auto& pkt : client_incoming) {
                client.reliability.process_incoming_header(pkt.header);
            }
        }

        // Small simulation sleep between ticks
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "\n================ SIMULATION RESULTS ================" << std::endl;
    std::cout << "  Total Server Ticks:        " << TOTAL_TICKS << std::endl;
    std::cout << "  Sub-Tick Inputs Sent:      " << total_inputs_generated << std::endl;
    std::cout << "  Sub-Tick Inputs Received:  " << total_inputs_processed << std::endl;
    std::cout << "  Average RTT:               " << std::fixed << std::setprecision(2)
              << clients[0].reliability.round_trip_time_ms() << " ms" << std::endl;
    std::cout << "  Uncompressed Full State:   " << full_bytes_total << " bytes" << std::endl;
    std::cout << "  Delta Compressed State:    " << delta_bytes_total << " bytes" << std::endl;

    double savings = (1.0 - (static_cast<double>(delta_bytes_total) / static_cast<double>(full_bytes_total))) * 100.0;
    std::cout << "  Bandwidth Savings:         " << savings << " %" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "[SUCCESS] Sub-tick netcode benchmark finished cleanly!" << std::endl;

    return 0;
}
