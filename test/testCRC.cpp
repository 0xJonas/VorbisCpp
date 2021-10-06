#include "util.h"
#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

static uint8_t* vectorToBuffer(const std::vector<uint8_t>& dataVec) {
    std::size_t size = dataVec.size();
    uint8_t* const data = new uint8_t[size];
    std::size_t index = 0;
    for (auto it = dataVec.begin(); it != dataVec.end(); it++) {
        data[index] = uint8_t(*it);
        index++;
    }
    return data;
}

RC_GTEST_PROP(testCRC, changing_a_single_bit_changes_crc,
    (const std::vector<uint8_t> dataVec, std::size_t bitOffset)) {

    const std::size_t size = dataVec.size();
    RC_PRE(size > 0);
    uint8_t* const data = vectorToBuffer(dataVec);

    const vcpp::CRC32 crc(0x04C11DB7);
    const uint32_t check1 = crc(data, size);

    bitOffset %= size * 8;
    const std::size_t byteOffset = bitOffset >> 3;
    const std::size_t shift = bitOffset & 0x7;
    data[byteOffset] ^= 0x80 >> shift;

    const uint32_t check2 = crc(data, size);
    RC_ASSERT(check1 != check2);
}

static void xorAtOffset(uint8_t* const data, const uint32_t polynomial, const std::size_t bitOffset) {
    RC_PRE(bitOffset > 0);

    const std::size_t byteOffset = bitOffset >> 3;
    const std::size_t shift = bitOffset & 0x7;
    data[byteOffset    ] ^= (polynomial >> (24 + shift)) & 0xff;
    data[byteOffset + 1] ^= (polynomial >> (16 + shift)) & 0xff;
    data[byteOffset + 2] ^= (polynomial >> (8 + shift)) & 0xff;
    data[byteOffset + 3] ^= (polynomial >> shift) & 0xff;
    data[byteOffset + 4] ^= (polynomial << (8 - shift)) & 0xff;

    // Add implicit 1.
    // CRC32 polynomials implicitly add a 1 before the MSB.
    // e.g. the polynomial denoted by 0x04C11DB7 is actually 0x104C11DB7.
    // This does not change the calculations for the CRC but it does have to considered here.
    if (shift == 0) {
        data[byteOffset - 1] ^= 1;
    }
    else {
        data[byteOffset] ^= 0x80 >> (shift - 1);
    }
}

RC_GTEST_PROP(testCRC, adding_polynomial_anywhere_does_not_change_crc,
    (const std::vector<uint8_t> dataVec, const std::size_t insert1, const std::size_t insert2, const std::size_t insert3)) {
    
    std::size_t size = dataVec.size();
    RC_PRE(size > 4);
    uint8_t* const data = vectorToBuffer(dataVec);

    // Calculate first CRC
    const uint32_t polynomial = 0x04C11DB7;
    vcpp::CRC32 crc(polynomial);
    uint32_t check1 = crc(data, size);

    // Add polynomial three times at random offsets
    xorAtOffset(data, polynomial, insert1 % (size * 8 - 32));
    xorAtOffset(data, polynomial, insert2 % (size * 8 - 32));
    xorAtOffset(data, polynomial, insert3 % (size * 8 - 32));
    uint32_t check2 = crc(data, size);

    RC_ASSERT(check1 == check2);
    delete[] data;
}

RC_GTEST_PROP(testCRC, checksums_can_be_merged_by_setting_initial_remainder_to_previous_crc,
    (const std::vector<uint8_t> dataVec, std::size_t split)) {

    const std::size_t size = dataVec.size();
    RC_PRE(size > 0);
    const uint8_t* const data = vectorToBuffer(dataVec);

    split %= size;
    const vcpp::CRC32 crc(0x04C11DB7);
    const uint32_t check1 = crc(data, size);

    const uint32_t intermediate = crc(data, split);
    const uint32_t check2 = crc(data + split, (size - split), intermediate);

    RC_ASSERT(check1 == check2);
}
