# subtick

[![CI](https://github.com/ErenYeager1836/subtick/actions/workflows/ci.yml/badge.svg)](https://github.com/ErenYeager1836/subtick/actions)
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Release](https://img.shields.io/github/v/release/ErenYeager1836/subtick)](https://github.com/ErenYeager1836/subtick/releases)

A high-performance, zero-dependency **C++20** reliable UDP game networking engine inspired by Valve's **Counter-Strike 2 sub-tick architecture** and Glenn Fiedler's virtual ACK bitfield model.

Designed for real-time multiplayer games that require millisecond-accurate physics, anti-lag compensation, and deterministic sub-tick event ordering over lossy UDP networks.

---

## Key Features

- **Lean 12-Byte Wire Header**: Magic Protocol ID (32-bit), Sequence (16-bit), Remote ACK (16-bit), and 32-bit ACK history bitfield.
- **Sliding Window Reliability**: Zero-redundancy ACK tracking without TCP-like head-of-line blocking or redundant ACK round-trips.
- **Microsecond Sub-Tick Queue**: Captures player actions at exact fractions within a server tick (`tick + fraction`).
- **Server Lag Compensation ("Valve Time-Rewind")**: Server maintains rolling hitbox histories, rewinds entities to exact sub-tick timestamps, and performs 3D Ray-AABB intersection tests.
- **Client Prediction & Rollback Reconciliation**: Predicts local movement instantaneously and automatically rolls back + re-simulates inputs upon authoritative server corrections.
- **MTU-Safe Fragmentation & Token Bucket Pacing**: Slices oversized snapshots into MTU-safe fragments with out-of-order reassembly and rate-limits bandwidth.
- **Jacobson/Karels RTT Estimation**: RFC 6298 smoothed round-trip time and jitter variance calculations.
- **Bit-Level Quantization & Delta Compression**: Compresses 32-bit floating point vectors and entity states using bitmask change fields.
- **Built-in Network Simulator**: Test suite runs against configurable packet loss, variable latency, and jitter directly in-memory.
- **Standard Library Only**: Zero third-party dependencies; pure modern C++20.

---

## Architecture

```
+-------------------------------------------------------------------------+
|                                 subtick                                 |
|               High-Performance C++20 Game Networking Engine             |
+-------------------------------------------------------------------------+
       |
       +--> [1. Packet Framing]   : 12-byte wire header (Magic, Seq, Ack, 32-bit mask)
       |
       +--> [2. Reliability Core] : Sliding window buffer + automatic message resends
       |
       +--> [3. Network Simulator]: Virtual socket with packet drop, jitter & latency
       |
       +--> [4. Sub-Tick Queue]   : Microsecond-precision input sorting within ticks
       |
       +--> [5. Delta Compression]: Dynamic state bitmasking & float quantization
       |
       +--> [6. Lag Compensation] : Historical hitbox rewinding & 3D raycast validation
       |
       +--> [7. Client Prediction]: Local movement simulation with server rollback
       |
       +--> [8. MTU Fragmentation]: Large payload slicing & token-bucket bandwidth pacing
```

---

## Wire Format

Every packet begins with a lean 12-byte header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Protocol ID (32 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Sequence (16 bits)     |           Ack (16 bits)       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Ack Bitfield (32 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Payload Bytes ...                     |
```

---

## Code Examples

### 1. Sub-Tick Input Generation & Chronological Sorting
In traditional 64-tick netcode, an attack clicked at `tick 64.3` was delayed until `tick 65.0`. In `subtick`, inputs capture microsecond sub-tick fractions:

```cpp
#include "subtick/subtick.hpp"

// Client creates a sub-tick event
subtick::SubTickInputEvent input;
input.time.tick = 64;
input.time.fraction = 0.354f; // Fired 35.4% into tick 64
input.action = subtick::ActionType::ATTACK;
input.view_angles = {12.5f, -85.2f, 0.0f};

// Serialize into bitstream with float quantization
subtick::BitWriter writer;
input.serialize(writer);

// Server queue sorts incoming inputs chronologically by sub-tick time:
subtick::SubTickQueue queue;
queue.push(input);

// Process all events belonging to tick 64 in microsecond order:
auto tick_events = queue.pop_events_for_tick(64);
```

### 2. Server-Side Lag Compensation (Valve Time-Rewind)
When an 80ms ping player fires, the server rewinds all enemy hitboxes back in time to where they were at the client's sub-tick timestamp:

```cpp
subtick::LagCompensator server_lag_comp;

// Server continuously records entity bounding boxes
server_lag_comp.record_entity_hitbox(enemy_id, {100, 0.0f, 100000}, enemy_aabb_at_100);
server_lag_comp.record_entity_hitbox(enemy_id, {101, 0.0f, 101000}, enemy_aabb_at_101);

// Client shot timestamp and 3D ray
subtick::SubTickTime shot_time{100, 0.5f, 100500}; // Sub-tick interpolated time
subtick::Ray shot_ray{{15.0f, -100.0f, 5.0f}, {0.0f, 1.0f, 0.0f}};

// Verify hit: server interpolates hitbox to tick 100.5 and casts ray
subtick::HitResult hit = server_lag_comp.verify_shot(shooter_id, shot_time, shot_ray);
if (hit.hit) {
    // Direct hit validated! Target entity: hit.target_entity_id at hit.hit_point
}
```

### 3. Client-Side Prediction & Server Reconciliation with Rollback
The client moves immediately; if the server reports an authoritative desync, the client snaps back and re-simulates unacknowledged commands:

```cpp
subtick::ClientPredictor predictor;

// 1. Client predicts movement immediately
subtick::PlayerInput in{cmd_number, tick, {1.0f, 0.0f, 0.0f}, 1.0f / 64.0f};
subtick::PlayerState predicted = predictor.predict(in);

// 2. Authoritative server snapshot arrives later:
bool rollback_occurred = predictor.reconcile(authoritative_server_state);
if (rollback_occurred) {
    // Prediction error exceeded threshold:
    // Automatically rolled back to server state and re-simulated subsequent inputs!
}
```

### 4. MTU-Safe Fragmentation & Reassembly
Slices large snapshot bursts into MTU-safe chunks and reassembles them regardless of delivery order:

```cpp
subtick::Fragmenter fragmenter(1100); // Max 1100 bytes per chunk
subtick::Reassembler reassembler;

// Large 50 KB game state
std::vector<uint8_t> large_state(50000, 0xAB);
auto chunks = fragmenter.split(large_state, message_id);

// Feed chunks to receiver (handles out-of-order arrivals):
for (const auto& chunk : chunks) {
    auto complete_payload = reassembler.ingest_fragment(chunk);
    if (complete_payload) {
        // Complete 50 KB payload reassembled!
    }
}
```

---

## Benchmark Results

Running the multi-client simulation benchmark ([`examples/benchmark_server_client.cpp`](examples/benchmark_server_client.cpp)) with **8 concurrent clients** over a lossy network (**5% packet loss, 50ms RTT, +/-5ms jitter**):

| Metric | Result |
|---|---|
| **Server Simulation Ticks** | 128 ticks (authoritative 64-tick loop) |
| **Sub-Tick Inputs Sent** | 1,024 inputs |
| **Sub-Tick Inputs Processed** | 920 inputs (with loss recovery) |
| **Smoothed RTT** | ~74 ms |
| **Uncompressed Full State** | 30,720 bytes |
| **Delta Compressed State** | 13,312 bytes |
| **Bandwidth Reduction** | **56.67% savings** |

---

## Building and Testing

### Prerequisites
- C++20 compatible compiler (MSVC 2022/2026, Clang 15+, GCC 12+)
- CMake 3.20+

### Build with CMake
```bash
cmake -B build -S .
cmake --build build --config Release
```

### Run All 9 Unit Tests
```bash
ctest --test-dir build -C Release --output-on-failure
```

### Run Multi-Client Benchmark Simulation
```bash
# Windows
./build/Release/benchmark_server_client.exe

# Linux
./build/benchmark_server_client
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
