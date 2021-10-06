#include "OggStream.h"
#include "util.h"

#include <string>
#include <istream>

using namespace vcpp;

void OggPacket::addNewPageCallback(NewPageCallback& callback) {
    callbacks.push_back(&callback);
}

void OggPhysicalStream::addNewStreamCallback(NewStreamCallback& callback) {
    newStreamCallbacks.push_back(&callback);
}

void OggPhysicalStream::addNewPacketCallback(uint32_t streamSerialNumber, NewPacketCallback& callback) {
    if (newPacketCallbacks.find(streamSerialNumber) == newPacketCallbacks.end()) {
        newPacketCallbacks.insert({ streamSerialNumber, std::vector<NewPacketCallback*>() });
    }

    std::vector<NewPacketCallback*>& callbacks = newPacketCallbacks.at(streamSerialNumber);
    callbacks.push_back(&callback);
}

static inline void oggAssert(
    bool condition, 
    const std::string& message,
    OggStreamError::Cause cause = vcpp::OggStreamError::Cause::Other
) {
    if (!condition) {
        throw OggStreamError(cause, message);
    }
}

OggPage OggPhysicalStream::readPage() {
    uint8_t headerData[23];
    in.read(headerData, 23);
    oggAssert(in.gcount() == 23, "Unexpected End Of File", OggStreamError::Cause::UnexpectedEOF);

    const uint8_t streamStructureVersion = headerData[0];
    oggAssert(streamStructureVersion == 0, "stream_structure_version should be 0.");

    const uint8_t headerTypeFlag = headerData[1];
    const bool continuedPacket = (headerTypeFlag & 0x01) != 0;
    const bool firstPage = (headerTypeFlag & 0x02) != 0;
    const bool lastPage = (headerTypeFlag & 0x04) != 0;

    const int64_t granulePosition = readUInt64LE(&headerData[2]);
    const uint32_t streamSerialNumber = readUInt32LE(&headerData[10]);
    const uint32_t pageSequenceNumber = readUInt32LE(&headerData[14]);
    const uint32_t pageChecksum = readUInt32LE(&headerData[18]);
    const uint8_t pageSegments = headerData[22];

    return OggPage(0);
}

template<typename CharT, typename Traits>
bool findSequence(std::basic_istream<CharT, Traits>& in, const CharT* sequence, const std::size_t size) {
    std::size_t matches = 0;
    while (matches < size) {
        const auto c = in.get();
        if (in.eof()) {
            return false;
        }

        if (Traits::to_char_type(c) == sequence[matches]) {
            matches++;
        }
    }
    return true;
}

void OggPhysicalStream::process() {
    const uint8_t pageHeader[4] = { 0x4f, 0x67, 0x67, 0x53 };   // "OggS"

    while (!in.eof()) {
        bool found = findSequence(in, pageHeader, 4);
        if (!found) {
            break;
        }
    }
}
