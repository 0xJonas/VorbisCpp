#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace vcpp {
    inline uint32_t readUInt32LE(const uint8_t* data) {
        return uint32_t(data[0])
            | (uint32_t(data[1]) << 8)
            | (uint32_t(data[2]) << 16)
            | (uint32_t(data[3]) << 24);
    }

    inline uint64_t readUInt64LE(const uint8_t* data) {
        return uint64_t(data[0])
            | (uint64_t(data[1]) << 8)
            | (uint64_t(data[2]) << 16)
            | (uint64_t(data[3]) << 24)
            | (uint64_t(data[4]) << 32)
            | (uint64_t(data[5]) << 40)
            | (uint64_t(data[6]) << 48)
            | (uint64_t(data[7]) << 56);
    }

    class CRC32 {
        const uint32_t* const lookupTable;

        static uint32_t* generateLookupTable(const uint32_t polynomial);
    public:
        CRC32(const uint32_t polynomial) : lookupTable(generateLookupTable(polynomial)) {}
        ~CRC32();

        uint32_t operator() (const uint8_t* const data, const std::size_t size, const uint32_t initialRemainder = 0) const;
    };
}

#endif