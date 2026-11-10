#pragma once

#include "subtick/config.hpp"
#include <vector>
#include <optional>
#include <cstdint>

namespace subtick {

template <typename T, size_t Size = SEQUENCE_BUFFER_SIZE>
class SequenceBuffer {
    static_assert((Size & (Size - 1)) == 0, "SequenceBuffer size must be a power of two");

public:
    SequenceBuffer()
        : sequence_numbers_(Size, 0xFFFFFFFF), entries_(Size) {}

    void reset() {
        std::fill(sequence_numbers_.begin(), sequence_numbers_.end(), 0xFFFFFFFF);
    }

    T* insert(uint16_t sequence) {
        size_t index = sequence & (Size - 1);
        sequence_numbers_[index] = sequence;
        return &entries_[index];
    }

    void remove(uint16_t sequence) {
        size_t index = sequence & (Size - 1);
        if (sequence_numbers_[index] == sequence) {
            sequence_numbers_[index] = 0xFFFFFFFF;
        }
    }

    [[nodiscard]] bool available(uint16_t sequence) const {
        size_t index = sequence & (Size - 1);
        return sequence_numbers_[index] == 0xFFFFFFFF;
    }

    [[nodiscard]] bool exists(uint16_t sequence) const {
        size_t index = sequence & (Size - 1);
        return sequence_numbers_[index] == sequence;
    }

    [[nodiscard]] T* find(uint16_t sequence) {
        size_t index = sequence & (Size - 1);
        if (sequence_numbers_[index] == sequence) {
            return &entries_[index];
        }
        return nullptr;
    }

    [[nodiscard]] const T* find(uint16_t sequence) const {
        size_t index = sequence & (Size - 1);
        if (sequence_numbers_[index] == sequence) {
            return &entries_[index];
        }
        return nullptr;
    }

    [[nodiscard]] uint32_t generate_ack_bitfield(uint16_t ack) const {
        uint32_t ack_bits = 0;
        for (int i = 1; i <= 32; ++i) {
            uint16_t prev_seq = static_cast<uint16_t>(ack - i);
            if (exists(prev_seq)) {
                ack_bits |= (1U << (i - 1));
            }
        }
        return ack_bits;
    }

    [[nodiscard]] size_t size() const { return Size; }

private:
    std::vector<uint32_t> sequence_numbers_;
    std::vector<T> entries_;
};

} // namespace subtick
