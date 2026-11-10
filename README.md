# subtick

[![CI](https://github.com/ErenYeager1836/subtick/actions/workflows/ci.yml/badge.svg)](https://github.com/ErenYeager1836/subtick/actions)
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance, zero-dependency **C++20** reliable UDP game networking engine inspired by Valve's **Counter-Strike 2 sub-tick architecture** and Glenn Fiedler's virtual ACK bitfield model.

Designed for real-time multiplayer games that require millisecond-accurate physics, anti-lag compensation, and deterministic sub-tick event ordering over lossy UDP networks.

---

## Key Features

- **Lean 12-Byte Wire Header**: Magic Protocol ID, 16-bit Sequence, 16-bit Remote ACK, and 32-bit ACK history bitfield.
- **Sliding Window Reliability**: Zero-redundancy ACK tracking without TCP-like blocking or redundant ACK roundtrips.
- **Microsecond Sub-Tick Queue**: Captures inputs and actions at exact fractions within a server tick (`tick + fraction`).
- **Jacobson/Karels RTT Estimation**: RFC 6298 smoothed round-trip time and jitter variance calculations.
- **Bit-Level Quantization & Delta Compression**: Compresses 32-bit floating point vectors and entity states using bitmask change fields.
- **Built-in Network Simulator**: Test suite runs against configurable packet loss, variable latency, and jitter.
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

## Building and Testing

### Prerequisites
- C++20 compatible compiler (MSVC 2022/2026, Clang 15+, GCC 12+)
- CMake 3.20+

### Build with CMake
```bash
cmake -B build -S .
cmake --build build --config Release
```

### Run All Unit Tests
```bash
ctest --test-dir build -C Release --output-on-failure
```

### Run Multi-Client Benchmark Simulation
```bash
./build/Release/benchmark_server_client
```
