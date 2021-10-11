#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <array>

namespace vcpp {
    inline uint32_t readUInt32LE(const uint8_t* const data) {
        return uint32_t(data[0])
            | (uint32_t(data[1]) << 8)
            | (uint32_t(data[2]) << 16)
            | (uint32_t(data[3]) << 24);
    }

    inline uint64_t readUInt64LE(const uint8_t* const data) {
        return uint64_t(data[0])
            | (uint64_t(data[1]) << 8)
            | (uint64_t(data[2]) << 16)
            | (uint64_t(data[3]) << 24)
            | (uint64_t(data[4]) << 32)
            | (uint64_t(data[5]) << 40)
            | (uint64_t(data[6]) << 48)
            | (uint64_t(data[7]) << 56);
    }

    inline void writeUInt32LE(uint8_t* const data, const uint32_t value) {
        data[0] = value & 0xff;
        data[1] = (value >> 8) & 0xff;
        data[2] = (value >> 16) & 0xff;
        data[3] = (value >> 24) & 0xff;
    }

    inline void writeUInt64LE(uint8_t* const data, const uint64_t value) {
        data[0] = value & 0xff;
        data[1] = (value >> 8) & 0xff;
        data[2] = (value >> 16) & 0xff;
        data[3] = (value >> 24) & 0xff;
        data[4] = (value >> 32) & 0xff;
        data[5] = (value >> 40) & 0xff;
        data[6] = (value >> 48) & 0xff;
        data[7] = (value >> 56) & 0xff;
    }

    class CRC32 {
        const std::shared_ptr<uint32_t[256]> lookupTable_;

        static std::shared_ptr<uint32_t[256]> generateLookupTable(const uint32_t polynomial);
    public:
        CRC32(const uint32_t polynomial) : lookupTable_(generateLookupTable(polynomial)) {}

        uint32_t operator() (const uint8_t* const data, const std::size_t size, const uint32_t initialRemainder = 0) const;
        uint32_t operator() (const uint8_t value, const uint32_t remainder = 0) const;
    };
}

#endif