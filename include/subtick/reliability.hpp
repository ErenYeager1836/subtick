#pragma once

#include "subtick/config.hpp"
#include "subtick/packet.hpp"
#include "subtick/sequence_buffer.hpp"
#include "subtick/clock.hpp"
#include <vector>
#include <deque>
#include <cstdint>
#include <algorithm>
#include <functional>

namespace subtick {

struct SentPacketRecord {
    uint64_t time_sent_us{0};
    bool acknowledged{false};
    std::vector<uint32_t> reliable_message_ids;
};

struct ReceivedPacketRecord {
    uint64_t time_received_us{0};
};

struct ReliableMessage {
    uint32_t id{0};
    std::vector<uint8_t> data;
    uint64_t last_sent_us{0};
    int resend_count{0};
};

class ReliabilitySystem {
public:
    using AckCallback = std::function<void(uint16_t sequence, double rtt_ms)>;
    using LossCallback = std::function<void(uint16_t sequence)>;

    ReliabilitySystem() = default;

    // Call before sending a packet to obtain the next sequence and current ack state
    PacketHeader prepare_outgoing_header() {
        PacketHeader header;
        header.protocol_id = PROTOCOL_ID;
        header.sequence = local_sequence_++;
        header.ack = remote_ack_;
        header.ack_bitfield = received_buffer_.generate_ack_bitfield(remote_ack_);

        // Record the sent packet for RTT and ACK tracking
        SentPacketRecord* record = sent_buffer_.insert(header.sequence);
        if (record) {
            record->time_sent_us = Clock::now_us();
            record->acknowledged = false;
            record->reliable_message_ids.clear();
        }

        ++stats_packets_sent_;
        return header;
    }

    // Process an incoming packet header, updating remote ACK and detecting losses
    bool process_incoming_header(const PacketHeader& header, AckCallback on_ack = nullptr, LossCallback on_loss = nullptr) {
        if (header.protocol_id != PROTOCOL_ID) {
            return false;
        }

        uint64_t now_us = Clock::now_us();
        ++stats_packets_received_;

        // Update remote sequence / ACK state
        if (sequence_greater_than(header.sequence, remote_ack_)) {
            remote_ack_ = header.sequence;
        }

        // Record receipt in our received buffer (for generating future ack bitfields)
        ReceivedPacketRecord* recv_record = received_buffer_.insert(header.sequence);
        if (recv_record) {
            recv_record->time_received_us = now_us;
        }

        // Process ACKs sent by remote peer
        process_ack(header.ack, now_us, on_ack);

        // Process the 32-bit ACK bitfield
        for (int i = 1; i <= 32; ++i) {
            if (header.ack_bitfield & (1U << (i - 1))) {
                uint16_t acked_seq = static_cast<uint16_t>(header.ack - i);
                process_ack(acked_seq, now_us, on_ack);
            }
        }

        // Clean up unacknowledged packets that have fallen out of the 32-sequence window
        detect_packet_losses(header.ack, on_loss);

        return true;
    }

    // Queue a reliable message to be transmitted until acknowledged
    uint32_t queue_reliable_message(std::span<const uint8_t> data) {
        uint32_t msg_id = next_message_id_++;
        ReliableMessage msg;
        msg.id = msg_id;
        msg.data.assign(data.begin(), data.end());
        msg.last_sent_us = 0;
        msg.resend_count = 0;
        pending_messages_.push_back(std::move(msg));
        return msg_id;
    }

    // Pack pending reliable messages into outgoing packet payload
    void pack_reliable_messages(BitWriter& writer, uint16_t outgoing_sequence) {
        SentPacketRecord* record = sent_buffer_.find(outgoing_sequence);
        uint16_t message_count = 0;

        // Count how many fit in payload space
        for (const auto& msg : pending_messages_) {
            if (writer.byte_size() + msg.data.size() + 6 > MAX_PAYLOAD_SIZE) break;
            ++message_count;
        }

        writer.write_u16(message_count);
        for (size_t i = 0; i < message_count; ++i) {
            auto& msg = pending_messages_[i];
            writer.write_u32(msg.id);
            writer.write_u16(static_cast<uint16_t>(msg.data.size()));
            writer.write_bytes(msg.data);
            msg.last_sent_us = Clock::now_us();
            ++msg.resend_count;

            if (record) {
                record->reliable_message_ids.push_back(msg.id);
            }
        }
    }

    // Unpack reliable messages from received payload
    std::vector<std::vector<uint8_t>> unpack_reliable_messages(BitReader& reader) {
        std::vector<std::vector<uint8_t>> received;
        uint16_t message_count = reader.read_u16();
        if (reader.has_overflowed()) return received;

        for (uint16_t i = 0; i < message_count; ++i) {
            uint32_t msg_id = reader.read_u32();
            uint16_t size = reader.read_u16();
            if (reader.has_overflowed()) break;

            reader.align();
            if (reader.bits_remaining() < static_cast<size_t>(size) * 8) break;

            std::vector<uint8_t> msg_data(size);
            for (size_t b = 0; b < size; ++b) {
                msg_data[b] = reader.read_u8();
            }

            // Deduplicate: check if message was already delivered
            if (!delivered_messages_.exists(static_cast<uint16_t>(msg_id & 0xFFFF))) {
                delivered_messages_.insert(static_cast<uint16_t>(msg_id & 0xFFFF));
                received.push_back(std::move(msg_data));
            }
        }
        return received;
    }

    [[nodiscard]] double round_trip_time_ms() const { return smoothed_rtt_ms_; }
    [[nodiscard]] double rtt_variance_ms() const { return rtt_var_ms_; }
    [[nodiscard]] float packet_loss_ratio() const {
        if (stats_packets_sent_ == 0) return 0.0f;
        return static_cast<float>(stats_packets_lost_) / static_cast<float>(stats_packets_sent_);
    }
    [[nodiscard]] uint16_t local_sequence() const { return local_sequence_; }
    [[nodiscard]] uint16_t remote_ack() const { return remote_ack_; }
    [[nodiscard]] size_t pending_reliable_count() const { return pending_messages_.size(); }

private:
    void process_ack(uint16_t sequence, uint64_t now_us, AckCallback on_ack) {
        SentPacketRecord* record = sent_buffer_.find(sequence);
        if (record && !record->acknowledged) {
            record->acknowledged = true;
            ++stats_packets_acked_;

            // Calculate RTT
            if (now_us > record->time_sent_us) {
                double rtt_ms = static_cast<double>(now_us - record->time_sent_us) / 1000.0;
                update_rtt(rtt_ms);

                if (on_ack) {
                    on_ack(sequence, rtt_ms);
                }
            }

            // Remove acknowledged reliable messages from pending queue
            for (uint32_t acked_msg_id : record->reliable_message_ids) {
                pending_messages_.erase(
                    std::remove_if(pending_messages_.begin(), pending_messages_.end(),
                        [acked_msg_id](const ReliableMessage& msg) {
                            return msg.id == acked_msg_id;
                        }),
                    pending_messages_.end()
                );
            }
        }
    }

    void detect_packet_losses(uint16_t latest_ack, LossCallback on_loss) {
        // Any packet older than latest_ack by > 32 sequences is considered lost
        if (latest_ack < 33) return;

        uint16_t oldest_active = static_cast<uint16_t>(latest_ack - 33);
        SentPacketRecord* record = sent_buffer_.find(oldest_active);
        if (record && !record->acknowledged) {
            ++stats_packets_lost_;
            if (on_loss) {
                on_loss(oldest_active);
            }
            sent_buffer_.remove(oldest_active);
        }
    }

    void update_rtt(double rtt_sample_ms) {
        if (rtt_sample_ms <= 0.0) return;

        if (smoothed_rtt_ms_ == 0.0) {
            smoothed_rtt_ms_ = rtt_sample_ms;
            rtt_var_ms_ = rtt_sample_ms / 2.0;
        } else {
            // RFC 6298 Jacobson/Karels algorithm:
            // RTTVAR = (1 - beta) * RTTVAR + beta * |SRTT - R'| (beta = 0.25)
            // SRTT = (1 - alpha) * SRTT + alpha * R' (alpha = 0.125)
            rtt_var_ms_ = 0.75 * rtt_var_ms_ + 0.25 * std::abs(smoothed_rtt_ms_ - rtt_sample_ms);
            smoothed_rtt_ms_ = 0.875 * smoothed_rtt_ms_ + 0.125 * rtt_sample_ms;
        }
    }

    uint16_t local_sequence_{0};
    uint16_t remote_ack_{0};
    uint32_t next_message_id_{1};

    SequenceBuffer<SentPacketRecord> sent_buffer_;
    SequenceBuffer<ReceivedPacketRecord> received_buffer_;
    SequenceBuffer<uint8_t> delivered_messages_;

    std::deque<ReliableMessage> pending_messages_;

    double smoothed_rtt_ms_{0.0};
    double rtt_var_ms_{0.0};

    uint64_t stats_packets_sent_{0};
    uint64_t stats_packets_received_{0};
    uint64_t stats_packets_acked_{0};
    uint64_t stats_packets_lost_{0};
};

} // namespace subtick
