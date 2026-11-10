#include "subtick/bitstream.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>

#define TEST_ASSERT(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " #expr "\n"; std::abort(); } } while(0)

void test_bit_packing() {
    subtick::BitWriter writer;
    writer.write_bits(0b101, 3);
    writer.write_bits(0b11, 2);
    writer.write_bits(0b0, 1);
    writer.write_u8(0xAB);
    writer.write_u16(0x1234);
    writer.write_u32(0xDEADBEEF);
    writer.write_string("ValveEngine");

    subtick::BitReader reader(writer.bytes());
    TEST_ASSERT(reader.read_bits(3) == 0b101);
    TEST_ASSERT(reader.read_bits(2) == 0b11);
    TEST_ASSERT(reader.read_bits(1) == 0b0);
    TEST_ASSERT(reader.read_u8() == 0xAB);
    TEST_ASSERT(reader.read_u16() == 0x1234);
    TEST_ASSERT(reader.read_u32() == 0xDEADBEEF);
    TEST_ASSERT(reader.read_string() == "ValveEngine");
    TEST_ASSERT(!reader.has_overflowed());

    std::cout << "[PASS] test_bit_packing" << std::endl;
}

void test_quantized_floats() {
    subtick::BitWriter writer;
    float original = 45.67f;
    writer.write_quantized_float(original, -180.0f, 180.0f, 16);

    subtick::BitReader reader(writer.bytes());
    float decoded = reader.read_quantized_float(-180.0f, 180.0f, 16);
    TEST_ASSERT(std::abs(original - decoded) < 0.01f);

    std::cout << "[PASS] test_quantized_floats" << std::endl;
}

int main() {
    std::cout << "--- Running Bitstream Tests ---" << std::endl;
    test_bit_packing();
    test_quantized_floats();
    return 0;
}
