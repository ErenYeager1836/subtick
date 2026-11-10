#pragma once

#include "subtick/config.hpp"
#include "subtick/clock.hpp"
#include "subtick/bitstream.hpp"
#include <span>
#include <vector>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <cstdint>

namespace subtick {

inline constexpr size_t DEFAULT_FRAGMENT_SIZE = 1100; // Well below 1200 MTU

struct FragmentHeader {
    uint16_t message_id{0};
    uint16_t fragment_index{0};
    uint16_t total_fragments{0};
    uint32_t total_size{0};

    void serialize(BitWriter& writer) const {
        writer.write_u16(message_id);
        writer.write_u16(fragment_index);
        writer.write_u16(total_fragments);
        writer.write_u32(total_size);
    }

    static std::optional<FragmentHeader> deserialize(BitReader& reader) {
        if (reader.bits_remaining() < 80) return std::nullopt; // 10 bytes = 80 bits
        FragmentHeader h;
        h.message_id = reader.read_u16();
        h.fragment_index = reader.read_u16();
        h.total_fragments = reader.read_u16();
        h.total_size = reader.read_u32();
        if (reader.has_overflowed() || h.total_fragments == 0 || h.fragment_index >= h.total_fragments) {
            return std::nullopt;
        }
        return h;
    }
};

struct Fragment {
    FragmentHeader header;
    std::vector<uint8_t> data;
};

// Splits large payloads into MTU-safe fragments
class Fragmenter {
public:
    explicit Fragmenter(size_t max_chunk_size = DEFAULT_FRAGMENT_SIZE)
        : max_chunk_size_(max_chunk_size) {}

    std::vector<std::vector<uint8_t>> split(std::span<const uint8_t> payload, uint16_t message_id) const {
        std::vector<std::vector<uint8_t>> result;
        if (payload.empty()) return result;

        size_t total_fragments = (payload.size() + max_chunk_size_ - 1) / max_chunk_size_;
        result.reserve(total_fragments);

        for (size_t i = 0; i < total_fragments; ++i) {
            size_t offset = i * max_chunk_size_;
            size_t chunk_len = std::min<size_t>(max_chunk_size_, payload.size() - offset);

            FragmentHeader header;
            header.message_id = message_id;
            header.fragment_index = static_cast<uint16_t>(i);
            header.total_fragments = static_cast<uint16_t>(total_fragments);
            header.total_size = static_cast<uint32_t>(payload.size());

            BitWriter writer;
            header.serialize(writer);
            writer.write_bytes({payload.data() + offset, chunk_len});

            result.emplace_back(writer.bytes().begin(), writer.bytes().end());
        }

        return result;
    }

private:
    size_t max_chunk_size_{DEFAULT_FRAGMENT_SIZE};
};

// Reassembles fragments back into the original payload
class Reassembler {
public:
    struct AssemblyState {
        uint32_t total_size{0};
        uint16_t total_fragments{0};
        uint16_t received_count{0};
        uint64_t last_updated_us{0};
        std::vector<bool> received_mask;
        std::vector<std::vector<uint8_t>> chunks;
    };

    explicit Reassembler(uint64_t timeout_us = 5'000'000) // 5 seconds expiration
        : timeout_us_(timeout_us) {}

    // Ingests a raw fragment; returns complete payload if all fragments are present
    std::optional<std::vector<uint8_t>> ingest_fragment(std::span<const uint8_t> data) {
        BitReader reader(data);
        auto opt_header = FragmentHeader::deserialize(reader);
        if (!opt_header) return std::nullopt;

        const auto& h = *opt_header;
        reader.align();
        std::vector<uint8_t> chunk(data.begin() + (reader.current_bit() / 8), data.end());

        uint64_t now_us = Clock::now_us();
        auto& state = assemblies_[h.message_id];

        if (state.total_fragments == 0) {
            state.total_size = h.total_size;
            state.total_fragments = h.total_fragments;
            state.received_count = 0;
            state.received_mask.assign(h.total_fragments, false);
            state.chunks.resize(h.total_fragments);
        }

        state.last_updated_us = now_us;

        if (!state.received_mask[h.fragment_index]) {
            state.received_mask[h.fragment_index] = true;
            state.chunks[h.fragment_index] = std::move(chunk);
            ++state.received_count;
        }

        // Check if all fragments arrived
        if (state.received_count == state.total_fragments) {
            std::vector<uint8_t> complete;
            complete.reserve(state.total_size);
            for (auto& c : state.chunks) {
                complete.insert(complete.end(), c.begin(), c.end());
            }
            assemblies_.erase(h.message_id);
            return complete;
        }

        cleanup_stale(now_us);
        return std::nullopt;
    }

    void cleanup_stale(uint64_t current_time_us = Clock::now_us()) {
        auto it = assemblies_.begin();
        while (it != assemblies_.end()) {
            if (current_time_us - it->second.last_updated_us > timeout_us_) {
                it = assemblies_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    uint64_t timeout_us_{5'000'000};
    std::unordered_map<uint16_t, AssemblyState> assemblies_;
};

// Token-Bucket rate limiter for congestion control and bandwidth smoothing
class TokenBucketLimiter {
public:
    TokenBucketLimiter(double bytes_per_sec, double max_burst_bytes)
        : rate_bytes_per_sec_(bytes_per_sec),
          max_tokens_(max_burst_bytes),
          tokens_(max_burst_bytes),
          last_update_us_(Clock::now_us()) {}

    bool can_send(size_t bytes) {
        update_tokens();
        return tokens_ >= static_cast<double>(bytes);
    }

    bool consume(size_t bytes) {
        update_tokens();
        if (tokens_ >= static_cast<double>(bytes)) {
            tokens_ -= static_cast<double>(bytes);
            return true;
        }
        return false;
    }

    [[nodiscard]] double available_tokens() {
        update_tokens();
        return tokens_;
    }

private:
    void update_tokens() {
        uint64_t now_us = Clock::now_us();
        double elapsed_sec = static_cast<double>(now_us - last_update_us_) / 1'000'000.0;
        last_update_us_ = now_us;
        tokens_ = std::min(max_tokens_, tokens_ + elapsed_sec * rate_bytes_per_sec_);
    }

    double rate_bytes_per_sec_;
    double max_tokens_;
    double tokens_;
    uint64_t last_update_us_;
};

} // namespace subtick
