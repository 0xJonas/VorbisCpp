#include "util.h"

#include <cstdint>
#include <memory>

using namespace vcpp;

static uint32_t crc32TableEntry(uint8_t mask, const uint32_t polynomial) {
    uint32_t out = 0;
    for (std::size_t i = 0; i < 8; i++) {
        if ((mask & 0x80) != 0) {
            out ^= polynomial << (7 - i);
            mask ^= polynomial >> 25;
        }
        mask <<= 1;
    }
    return out;
}

std::shared_ptr<uint32_t[256]> CRC32::generateLookupTable(const uint32_t polynomial) {
    const std::shared_ptr<uint32_t[256]> table{ new uint32_t[256] };
    for (std::size_t i{ 0 }; i < 256; i++) {
        table[i] = crc32TableEntry(uint8_t(i), polynomial);
    }
    return table;
}

uint32_t CRC32::operator() (
    const uint8_t* const data,
    const std::size_t size,
    const uint32_t initialRemainder
) const {
    uint32_t remainder{ initialRemainder };
    for (std::size_t i{ 0 }; i < size; i++) {
        remainder = operator()(data[i], remainder);
    }
    return remainder;
}

uint32_t CRC32::operator() (const uint8_t value, const uint32_t remainder) const {
    return (remainder << 8) ^ lookupTable_[value ^ (remainder >> 24)];
}
