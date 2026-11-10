#include "subtick/reliability.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_ack_bitfield_and_sequence_window() {
    subtick::ReliabilitySystem client;
    subtick::ReliabilitySystem server;

    // Client sends packet 0, 1, 2, 3
    auto p0 = client.prepare_outgoing_header();
    auto p1 = client.prepare_outgoing_header();
    auto p2 = client.prepare_outgoing_header();
    auto p3 = client.prepare_outgoing_header();

    // Server receives 0, 1, and 3 (simulating packet 2 lost!)
    server.process_incoming_header(p0);
    server.process_incoming_header(p1);
    // packet 2 dropped
    server.process_incoming_header(p3);

    TEST_ASSERT(server.remote_ack() == 3);

    // Server sends back a response acknowledging up to 3
    auto server_header = server.prepare_outgoing_header();
    TEST_ASSERT(server_header.ack == 3);

    // Bit 0 corresponds to ack - 1 (packet 2): dropped (0)
    // Bit 1 corresponds to ack - 2 (packet 1): received (1)
    // Bit 2 corresponds to ack - 3 (packet 0): received (1)
    TEST_ASSERT((server_header.ack_bitfield & (1U << 0)) == 0);
    TEST_ASSERT((server_header.ack_bitfield & (1U << 1)) != 0);
    TEST_ASSERT((server_header.ack_bitfield & (1U << 2)) != 0);

    std::cout << "[PASS] test_ack_bitfield_and_sequence_window" << std::endl;
}

void test_reliable_message_delivery() {
    subtick::ReliabilitySystem client;
    subtick::ReliabilitySystem server;

    std::string secret = "FireWeaponEvent";
    std::vector<uint8_t> payload(secret.begin(), secret.end());

    uint32_t msg_id = client.queue_reliable_message(payload);
    TEST_ASSERT(msg_id != 0);
    TEST_ASSERT(client.pending_reliable_count() == 1);

    // Client prepares packet and packs reliable messages
    auto header = client.prepare_outgoing_header();
    subtick::BitWriter writer;
    client.pack_reliable_messages(writer, header.sequence);

    // Server receives and unpacks
    server.process_incoming_header(header);
    subtick::BitReader reader(writer.bytes());
    auto delivered = server.unpack_reliable_messages(reader);

    TEST_ASSERT(delivered.size() == 1);
    std::string received_str(delivered[0].begin(), delivered[0].end());
    TEST_ASSERT(received_str == secret);

    // Now server sends ACK back
    auto ack_header = server.prepare_outgoing_header();
    client.process_incoming_header(ack_header);

    // Client should now have 0 pending reliable messages because it was ACKed!
    TEST_ASSERT(client.pending_reliable_count() == 0);

    std::cout << "[PASS] test_reliable_message_delivery" << std::endl;
}

int main() {
    std::cout << "--- Running Reliability Tests ---" << std::endl;
    test_ack_bitfield_and_sequence_window();
    test_reliable_message_delivery();
    return 0;
}
