#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace subtick {

// Magic protocol identifier: 'S' 'U' 'B' 'T' = 0x53554254
inline constexpr uint32_t PROTOCOL_ID = 0x53554254;
inline constexpr uint16_t PROTOCOL_VERSION_MAJOR = 1;
inline constexpr uint16_t PROTOCOL_VERSION_MINOR = 0;

// Maximum Transmission Unit for real-time game UDP
inline constexpr size_t MAX_PACKET_SIZE = 1200; // Safe below standard 1500 MTU
inline constexpr size_t HEADER_SIZE = 12;       // Protocol (4) + Seq (2) + Ack (2) + Bitfield (4)
inline constexpr size_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;

// Sliding window sequence buffer depth (must be power of two)
inline constexpr size_t SEQUENCE_BUFFER_SIZE = 256;

// Sequence number arithmetic: 16-bit sequence comparison handling wraparound
inline bool sequence_greater_than(uint16_t s1, uint16_t s2) {
    return ((s1 > s2) && (s1 - s2 <= 32768)) ||
           ((s1 < s2) && (s2 - s1 > 32768));
}

inline bool sequence_less_than(uint16_t s1, uint16_t s2) {
    return sequence_greater_than(s2, s1);
}

inline uint16_t sequence_difference(uint16_t s1, uint16_t s2) {
    if (s1 >= s2) {
        return s1 - s2;
    }
    return static_cast<uint16_t>((65536 + s1) - s2);
}

} // namespace subtick
