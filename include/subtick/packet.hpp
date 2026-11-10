#pragma once

#include "subtick/config.hpp"
#include "subtick/bitstream.hpp"
#include <span>
#include <vector>
#include <optional>

namespace subtick {

struct PacketHeader {
    uint32_t protocol_id{PROTOCOL_ID};
    uint16_t sequence{0};
    uint16_t ack{0};
    uint32_t ack_bitfield{0};

    void write(BitWriter& writer) const {
        writer.write_u32(protocol_id);
        writer.write_u16(sequence);
        writer.write_u16(ack);
        writer.write_u32(ack_bitfield);
    }

    static std::optional<PacketHeader> read(BitReader& reader) {
        PacketHeader header;
        header.protocol_id = reader.read_u32();
        if (reader.has_overflowed() || header.protocol_id != PROTOCOL_ID) {
            return std::nullopt;
        }
        header.sequence = reader.read_u16();
        header.ack = reader.read_u16();
        header.ack_bitfield = reader.read_u32();
        if (reader.has_overflowed()) {
            return std::nullopt;
        }
        return header;
    }
};

struct Packet {
    PacketHeader header;
    std::vector<uint8_t> payload;

    [[nodiscard]] std::vector<uint8_t> serialize() const {
        BitWriter writer(HEADER_SIZE + payload.size());
        header.write(writer);
        if (!payload.empty()) {
            writer.write_bytes(payload);
        }
        return std::vector<uint8_t>(writer.bytes().begin(), writer.bytes().end());
    }

    static std::optional<Packet> deserialize(std::span<const uint8_t> data) {
        if (data.size() < HEADER_SIZE) {
            return std::nullopt;
        }
        BitReader reader(data);
        auto opt_header = PacketHeader::read(reader);
        if (!opt_header) {
            return std::nullopt;
        }
        Packet pkt;
        pkt.header = *opt_header;
        reader.align();
        size_t payload_bytes = data.size() - HEADER_SIZE;
        if (payload_bytes > 0) {
            pkt.payload.assign(data.begin() + HEADER_SIZE, data.end());
        }
        return pkt;
    }
};

} // namespace subtick
