#pragma once

#include "subtick/config.hpp"
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace subtick {

class BitWriter {
public:
    explicit BitWriter(size_t initial_capacity = MAX_PACKET_SIZE) {
        buffer_.reserve(initial_capacity);
    }

    void write_bits(uint32_t value, int num_bits) {
        if (num_bits <= 0 || num_bits > 32) return;
        value &= (num_bits == 32) ? 0xFFFFFFFF : ((1U << num_bits) - 1);

        for (int i = 0; i < num_bits; ++i) {
            uint8_t bit = (value >> i) & 1U;
            if (bit_index_ % 8 == 0) {
                buffer_.push_back(0);
            }
            buffer_.back() |= (bit << (bit_index_ % 8));
            ++bit_index_;
        }
    }

    void write_u8(uint8_t val) { write_bits(val, 8); }
    void write_u16(uint16_t val) { write_bits(val, 16); }
    void write_u32(uint32_t val) { write_bits(val, 32); }
    void write_u64(uint64_t val) {
        write_bits(static_cast<uint32_t>(val & 0xFFFFFFFF), 32);
        write_bits(static_cast<uint32_t>(val >> 32), 32);
    }

    void write_i8(int8_t val) { write_u8(static_cast<uint8_t>(val)); }
    void write_i16(int16_t val) { write_u16(static_cast<uint16_t>(val)); }
    void write_i32(int32_t val) { write_u32(static_cast<uint32_t>(val)); }

    void write_bool(bool val) { write_bits(val ? 1 : 0, 1); }

    // Quantized float compression: saves 32-bit floats in arbitrary bit-width
    void write_quantized_float(float val, float min_val, float max_val, int num_bits) {
        float clamped = std::clamp(val, min_val, max_val);
        float normalized = (clamped - min_val) / (max_val - min_val);
        uint32_t max_int = (num_bits == 32) ? 0xFFFFFFFF : ((1U << num_bits) - 1);
        uint32_t quantized = static_cast<uint32_t>(std::round(normalized * max_int));
        write_bits(quantized, num_bits);
    }

    void write_bytes(std::span<const uint8_t> data) {
        align();
        for (uint8_t b : data) {
            buffer_.push_back(b);
            bit_index_ += 8;
        }
    }

    void write_string(std::string_view str) {
        uint16_t len = static_cast<uint16_t>(std::min<size_t>(str.size(), 65535));
        write_u16(len);
        align();
        for (char c : str.substr(0, len)) {
            buffer_.push_back(static_cast<uint8_t>(c));
            bit_index_ += 8;
        }
    }

    void align() {
        int rem = bit_index_ % 8;
        if (rem != 0) {
            bit_index_ += (8 - rem);
        }
    }

    [[nodiscard]] const uint8_t* data() const { return buffer_.data(); }
    [[nodiscard]] size_t byte_size() const { return (bit_index_ + 7) / 8; }
    [[nodiscard]] size_t bit_size() const { return bit_index_; }
    [[nodiscard]] std::span<const uint8_t> bytes() const {
        return {buffer_.data(), byte_size()};
    }

    void reset() {
        buffer_.clear();
        bit_index_ = 0;
    }

private:
    std::vector<uint8_t> buffer_;
    size_t bit_index_{0};
};

class BitReader {
public:
    explicit BitReader(std::span<const uint8_t> data)
        : data_(data), total_bits_(data.size() * 8) {}

    uint32_t read_bits(int num_bits) {
        if (num_bits <= 0 || num_bits > 32) return 0;
        if (bit_index_ + num_bits > total_bits_) {
            overflow_ = true;
            return 0;
        }

        uint32_t value = 0;
        for (int i = 0; i < num_bits; ++i) {
            size_t byte_idx = (bit_index_ + i) / 8;
            int bit_offset = (bit_index_ + i) % 8;
            uint8_t bit = (data_[byte_idx] >> bit_offset) & 1U;
            value |= (static_cast<uint32_t>(bit) << i);
        }

        bit_index_ += num_bits;
        return value;
    }

    uint8_t read_u8() { return static_cast<uint8_t>(read_bits(8)); }
    uint16_t read_u16() { return static_cast<uint16_t>(read_bits(16)); }
    uint32_t read_u32() { return read_bits(32); }
    uint64_t read_u64() {
        uint64_t low = read_bits(32);
        uint64_t high = read_bits(32);
        return low | (high << 32);
    }

    int8_t read_i8() { return static_cast<int8_t>(read_u8()); }
    int16_t read_i16() { return static_cast<int16_t>(read_u16()); }
    int32_t read_i32() { return static_cast<int32_t>(read_u32()); }

    bool read_bool() { return read_bits(1) != 0; }

    float read_quantized_float(float min_val, float max_val, int num_bits) {
        uint32_t quantized = read_bits(num_bits);
        uint32_t max_int = (num_bits == 32) ? 0xFFFFFFFF : ((1U << num_bits) - 1);
        float normalized = static_cast<float>(quantized) / static_cast<float>(max_int);
        return min_val + normalized * (max_val - min_val);
    }

    void align() {
        int rem = bit_index_ % 8;
        if (rem != 0) {
            bit_index_ += (8 - rem);
        }
    }

    std::string read_string() {
        uint16_t len = read_u16();
        if (overflow_) return {};
        align();
        size_t byte_idx = bit_index_ / 8;
        if (byte_idx + len > data_.size()) {
            overflow_ = true;
            return {};
        }
        std::string result(reinterpret_cast<const char*>(data_.data() + byte_idx), len);
        bit_index_ += len * 8;
        return result;
    }

    [[nodiscard]] bool has_overflowed() const { return overflow_; }
    [[nodiscard]] size_t bits_remaining() const {
        return (bit_index_ < total_bits_) ? (total_bits_ - bit_index_) : 0;
    }
    [[nodiscard]] size_t current_bit() const { return bit_index_; }

private:
    std::span<const uint8_t> data_;
    size_t total_bits_{0};
    size_t bit_index_{0};
    bool overflow_{false};
};

} // namespace subtick
