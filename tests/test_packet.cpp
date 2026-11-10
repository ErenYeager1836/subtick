#include "subtick/packet.hpp"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_packet_serialization() {
    subtick::Packet pkt;
    pkt.header.protocol_id = subtick::PROTOCOL_ID;
    pkt.header.sequence = 1052;
    pkt.header.ack = 1048;
    pkt.header.ack_bitfield = 0b1101;
    pkt.payload = {'H', 'E', 'L', 'L', 'O'};

    auto wire_data = pkt.serialize();
    TEST_ASSERT(wire_data.size() == subtick::HEADER_SIZE + 5);

    auto deserialized = subtick::Packet::deserialize(wire_data);
    TEST_ASSERT(deserialized.has_value());
    TEST_ASSERT(deserialized->header.protocol_id == subtick::PROTOCOL_ID);
    TEST_ASSERT(deserialized->header.sequence == 1052);
    TEST_ASSERT(deserialized->header.ack == 1048);
    TEST_ASSERT(deserialized->header.ack_bitfield == 0b1101);
    TEST_ASSERT(deserialized->payload.size() == 5);
    TEST_ASSERT(deserialized->payload[0] == 'H');

    std::cout << "[PASS] test_packet_serialization" << std::endl;
}

void test_invalid_packet_rejected() {
    std::vector<uint8_t> corrupted(16, 0);
    auto deserialized = subtick::Packet::deserialize(corrupted);
    TEST_ASSERT(!deserialized.has_value());

    std::cout << "[PASS] test_invalid_packet_rejected" << std::endl;
}

int main() {
    std::cout << "--- Running Packet Tests ---" << std::endl;
    test_packet_serialization();
    test_invalid_packet_rejected();
    return 0;
}
