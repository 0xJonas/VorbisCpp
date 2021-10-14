#include "OggStream.h"
#include "util.h"

#include <string>
#include <algorithm>
#include <istream>
#include <cmath>
#include <cstdint>

using namespace vcpp;

const CRC32 oggCRC(0x04C11DB7);

const unsigned int maxPageSize = 255 * 255;

static const uint8_t capturePattern[4] { 0x4f, 0x67, 0x67, 0x53 };   // "OggS"

static inline void oggAssert(
    bool condition,
    const std::string& message,
    OggStreamError::Cause cause = vcpp::OggStreamError::Cause::Other
) {
    if (!condition) {
        throw OggStreamError{ cause, message };
    }
}

//----------------------------------------------
//                   OggPage
//----------------------------------------------

OggPage::OggPage(OggPage::Params&& params)
    : isContinuedPacket{ std::move(params.isContinuedPacket) },
      isFirstPage{ std::move(params.isFirstPage) },
      isLastPage{ std::move(params.isLastPage) },
      granulePosition{ std::move(params.granulePosition) },
      streamSerialNumber{ std::move(params.streamSerialNumber) },
      pageSequenceNumber{ std::move(params.pageSequenceNumber) },
      pageChecksum{ std::move(params.pageChecksum) },
      dataSize{ std::move(params.dataSize) },
      data{ std::move(params.data) } {
    params.data = nullptr;
}

//----------------------------------------------
//             OggLogicalStreamIn
//----------------------------------------------

OggLogicalStreamIn::OggLogicalStreamIn(uint32_t streamSerialNumber)
    : streamSerialNumber_{ streamSerialNumber },
      granulePosition_{ -1 }, 
      pageSequenceNumber_{ 0 },
      isOpen_{ false } {}

void OggLogicalStreamIn::addDataCallback(const std::shared_ptr<DataCallback> callback) {
    dataCallbacks_.emplace_back(callback);
}

void OggLogicalStreamIn::removeDataCallback(const std::shared_ptr<DataCallback>& callback) {
    auto callbackIt{ find(dataCallbacks_.cbegin(), dataCallbacks_.cend(), callback) };
    dataCallbacks_.erase(callbackIt);
}

void OggLogicalStreamIn::processPage(const OggPage& page) {
    unsigned int numSkippedPages{ 0 };

    if (!page.isFirstPage) {
        oggAssert(
            page.pageSequenceNumber > pageSequenceNumber_,
            "Page sequence number is lower than expected.",
            OggStreamError::Cause::LatePage
        );
        numSkippedPages = page.pageSequenceNumber - (pageSequenceNumber_ + 1);
    }

    const MetaData meta {
        page.granulePosition,
        numSkippedPages,
        page.isFirstPage,
        page.isContinuedPacket,
        page.isLastPage
    };
    for (std::shared_ptr<DataCallback>& callback : dataCallbacks_) {
        callback->onDataAvailable(page.data.get(), page.dataSize, meta);
    }

    pageSequenceNumber_ = page.pageSequenceNumber;
}

//----------------------------------------------
//            OggPhysicalStreamIn
//----------------------------------------------

OggPhysicalStreamIn::FileInput::FileInput(FILE* file) : file_{ file } {}

uint8_t OggPhysicalStreamIn::FileInput::read() {
    uint8_t out{ uint8_t(fgetc(file_)) };
    if (ferror(file_)) {
        throw OggStreamError(OggStreamError::Cause::IOError, "IOError occured.");
    }
    return out;
}

std::size_t OggPhysicalStreamIn::FileInput::read(uint8_t* const buffer, const std::size_t count) {
    std::size_t numChars{ fread(buffer, sizeof(uint8_t), count, file_) };
    if (ferror(file_)) {
        throw OggStreamError(OggStreamError::Cause::IOError, "IOError occured.");
    }
    return numChars;
}

bool OggPhysicalStreamIn::FileInput::eof() const {
    return feof(file_);
}

OggPhysicalStreamIn::StreamInput::StreamInput(std::basic_istream<uint8_t>& in) : in_{ in } {}

uint8_t OggPhysicalStreamIn::StreamInput::read() {
    uint8_t out{ uint8_t(in_.get()) };
    if (in_.fail() && !in_.eof()) {
        throw OggStreamError(OggStreamError::Cause::IOError, "IOError occured.");
    }
    return out;
}

std::size_t OggPhysicalStreamIn::StreamInput::read(uint8_t* const buffer, const std::size_t count) {
    in_.read(buffer, count);
    if (in_.fail() && !in_.eof()) {
        throw OggStreamError(OggStreamError::Cause::IOError, "IOError occured.");
    }
    return in_.gcount();
}

bool OggPhysicalStreamIn::StreamInput::eof() const {
    return in_.eof();
}

OggPhysicalStreamIn::OggPhysicalStreamIn(std::basic_istream<uint8_t>& in) : input_{ std::make_unique<StreamInput>(in) } {}

OggPhysicalStreamIn::OggPhysicalStreamIn(FILE* file) : input_{ std::make_unique<FileInput>(file) } {}

void OggPhysicalStreamIn::addNewStreamCallback(const std::shared_ptr<NewStreamCallback> callback) {
    newStreamCallbacks_.emplace_back(callback);
}

void OggPhysicalStreamIn::removeNewStreamCallback(const std::shared_ptr<NewStreamCallback>& callback) {
    auto callbackIt{ find(newStreamCallbacks_.cbegin(), newStreamCallbacks_.cend(), callback) };
    newStreamCallbacks_.erase(callbackIt);
}

OggPage OggPhysicalStreamIn::readPage() {
    uint8_t headerData[23];
    oggAssert(input_->read(headerData, 23) == 23, "Unexpected End Of File", OggStreamError::Cause::UnexpectedEOF);

    OggPage::Params params{};
    params.streamStructureVersion = headerData[0];
    const uint8_t headerTypeFlag{ headerData[1] };
    params.isContinuedPacket = (headerTypeFlag & 0x01) != 0;
    params.isFirstPage = (headerTypeFlag & 0x02) != 0;
    params.isLastPage = (headerTypeFlag & 0x04) != 0;
    params.granulePosition = readUInt64LE(&headerData[2]);
    params.streamSerialNumber = readUInt32LE(&headerData[10]);
    params.pageSequenceNumber = readUInt32LE(&headerData[14]);
    params.pageChecksum = readUInt32LE(&headerData[18]);

    oggAssert(params.streamStructureVersion == 0, "stream_structure_version should be 0.");

    uint32_t checksum{ oggCRC(capturePattern, 4) };
    headerData[18] = 0;
    headerData[19] = 0;
    headerData[20] = 0;
    headerData[21] = 0;
    checksum = oggCRC(headerData, 23, checksum);

    const uint8_t pageSegments = headerData[22];

    uint8_t segmentTable[256];
    oggAssert(
        input_->read(segmentTable, pageSegments) == pageSegments,
        "Unexpected End Of File", 
        OggStreamError::Cause::UnexpectedEOF
    );
    checksum = oggCRC(segmentTable, pageSegments, checksum);

    std::size_t dataSize{ 0 };
    for (std::size_t i{ 0 }; i < pageSegments; i++) {
        dataSize += segmentTable[i];
    }
    params.dataSize = dataSize;
    params.data = std::unique_ptr<uint8_t[]>{ new uint8_t[dataSize] };

#define BLOCK_SIZE 0x2000
    const auto eof = std::basic_istream<uint8_t>::traits_type::eof();
    const std::size_t strip{ dataSize % BLOCK_SIZE };
    for (std::size_t i{ 0 }; i < dataSize - strip; i += BLOCK_SIZE) {
        oggAssert(
            input_->read(&params.data[i], BLOCK_SIZE) == BLOCK_SIZE,
            "Unexpected End Of File",
            OggStreamError::Cause::UnexpectedEOF
        );
        checksum = oggCRC(&params.data[i], BLOCK_SIZE, checksum);
    }
    oggAssert(
        input_->read(&params.data[dataSize - strip], strip) == int(strip),
        "Unexpected End Of File",
        OggStreamError::Cause::UnexpectedEOF
    );

    checksum = oggCRC(&params.data[dataSize - strip], strip, checksum);
#undef BLOCK_SIZE

    oggAssert(checksum == params.pageChecksum, "Bad checksum.", OggStreamError::Cause::BadChecksum);

    return OggPage(std::move(params));
}

void OggPhysicalStreamIn::resync() {
    std::size_t matches{ 0 };
    std::size_t capturePatternLength = sizeof(capturePattern) / sizeof(uint8_t);
    while (matches < capturePatternLength && !input_->eof()) {
        const uint8_t c{ input_->read() };
        if (capturePattern[matches] == c) {
            matches++;
        }
        else if (capturePattern[0] == c) {
            matches = 1;
        }
        else {
            matches = 0;
        }
    }
}

void OggPhysicalStreamIn::process() {
    resync();
    while (!input_->eof()) {
        const OggPage page{ readPage() };

        auto logicalStreamIt{ logicalStreams_.find(page.streamSerialNumber) };
        if (logicalStreamIt != logicalStreams_.end()) {
            logicalStreamIt->second.processPage(page);
        }
        else {
            logicalStreams_.emplace(page.streamSerialNumber, page.streamSerialNumber);
            OggLogicalStreamIn& newStream{ logicalStreams_.find(page.streamSerialNumber)->second };
            for (std::shared_ptr<NewStreamCallback>& callback : newStreamCallbacks_) {
                callback->onNewStream(newStream);
            }
            newStream.processPage(page);
        }

        resync();
    }
}

//----------------------------------------------
//            OggLogicalStreamOut
//----------------------------------------------

OggLogicalStreamOut::OggLogicalStreamOut(OggPhysicalStreamOut& sink, const uint32_t streamSerialNumber) :
    sink_{ sink },
    streamSerialNumber_{ streamSerialNumber },
    pageSequenceNumber_{ 0 },
    isPacketOpen_{ false },
    isStreamOpen_{ true },
    isFirstWrite_{ true } {}

OggLogicalStreamOut::OggLogicalStreamOut(OggLogicalStreamOut&& other) noexcept
    : sink_{ other.sink_ },
      streamSerialNumber_{ std::move(other.streamSerialNumber_) },
      pageSequenceNumber_{ std::move(other.pageSequenceNumber_) },
      isPacketOpen_{ std::move(other.isPacketOpen_) },
      isStreamOpen_{ std::move(other.isStreamOpen_) },
      isFirstWrite_{ std::move(other.isFirstWrite_) } {}

void OggLogicalStreamOut::writePage(
        const uint8_t* const data,
        const unsigned int size,
        const int64_t granulePosition,
        const bool closePacket,
        const bool closeStream) {
    if (!isStreamOpen_) {
        throw OggStreamError(OggStreamError::Cause::StreamClosed, "Attempting to write to a closed stream.");
    }
    if (size > maxPageSize) {
        throw OggStreamError(OggStreamError::Cause::Other, "Too much data for a single page.");
    }

    uint32_t checksum{ oggCRC(capturePattern, 4) };

    uint8_t headerData[23];
    headerData[0] = 0; // stream_structure_version
    uint8_t headerTypeFlag{ uint8_t((isPacketOpen_ ? 0x1 : 0) + (isFirstWrite_ ? 0x2 : 0) + (closeStream ? 0x4 : 0)) };
    headerData[1] = headerTypeFlag;
    writeUInt64LE(&headerData[2], granulePosition);
    writeUInt32LE(&headerData[10], streamSerialNumber_);
    writeUInt32LE(&headerData[14], pageSequenceNumber_);
    writeUInt32LE(&headerData[18], 0); // checksum
    const uint8_t pageSegments{ uint8_t((size + 254) / 255) };
    headerData[22] = pageSegments;
    checksum = oggCRC(headerData, 23, checksum);

    uint8_t segmentTable[256];
    if (pageSegments > 0) {
        for (std::size_t i = 0; i < pageSegments - 1; i++) {
            segmentTable[i] = 255;
        }

        const uint8_t lastSegment{ size % 255 };
        if (lastSegment == 0) {
            segmentTable[pageSegments - 1] = 255;
        }
        else {
            segmentTable[pageSegments - 1] = lastSegment;
        }
        checksum = oggCRC(segmentTable, pageSegments, checksum);
    }

    checksum = oggCRC(data, size, checksum);

    writeUInt32LE(&headerData[18], checksum);

    sink_.writeLock.lock();
    sink_.output_->write(capturePattern, 4);
    sink_.output_->write(headerData, 23);
    sink_.output_->write(segmentTable, pageSegments);
    sink_.output_->write(data, size);
    sink_.writeLock.unlock();

    pageSequenceNumber_++;
    isFirstWrite_ = false;
    isPacketOpen_ = !closePacket;
}

void OggLogicalStreamOut::write(
        const uint8_t* const data,
        const unsigned int size,
        const int64_t granulePosition,
        const bool closePacket,
        const bool closeStream) {
    const std::ldiv_t sizeDiv{ ldiv(size, maxPageSize) };

    std::size_t bytesWritten{ 0 };
    for (std::size_t i = 0; i < sizeDiv.quot; i++) {
        writePage(&data[bytesWritten], maxPageSize, granulePosition, false, false);
        bytesWritten += maxPageSize;
    }
    writePage(&data[bytesWritten], sizeDiv.rem, granulePosition, closePacket, closeStream);
}

//----------------------------------------------
//            OggPhysicalStreamOut
//----------------------------------------------

OggPhysicalStreamOut::FileOutput::FileOutput(FILE* file) : file_{ file } {}

void OggPhysicalStreamOut::FileOutput::write(const uint8_t val) {
    fputc(val, file_);
}

void OggPhysicalStreamOut::FileOutput::write(const uint8_t* buffer, const std::size_t count) {
    fwrite(buffer, sizeof(uint8_t), count, file_);
}

OggPhysicalStreamOut::StreamOutput::StreamOutput(std::basic_ostream<uint8_t>& out) : out_{ out } {}

void OggPhysicalStreamOut::StreamOutput::write(const uint8_t val) {
    out_.put(val);
}

void OggPhysicalStreamOut::StreamOutput::write(const uint8_t* buffer, const std::size_t count) {
    out_.write(buffer, count);
}

OggPhysicalStreamOut::OggPhysicalStreamOut(FILE* file) 
    : output_ { std::make_unique<FileOutput>(file) } {}

OggPhysicalStreamOut::OggPhysicalStreamOut(std::basic_ostream<uint8_t>& out) 
    : output_{ std::make_unique<StreamOutput>(out) } {}

static uint32_t lfsrNext(const uint32_t lfsr) {
    unsigned int bit{ (lfsr ^ (lfsr >> 1) ^ (lfsr >> 21) ^ (lfsr >> 31)) & 1 };
    return (lfsr << 1) + bit;
}

OggLogicalStreamOut OggPhysicalStreamOut::newLogicalStream() {
    auto maxStreamSerialNum{ std::max_element(assignedSerialNums_.cbegin(), assignedSerialNums_.cend()) };
    uint32_t streamSerialNumber{ maxStreamSerialNum == assignedSerialNums_.cend() ? 1 : lfsrNext(*maxStreamSerialNum) };

    while (assignedSerialNums_.find(streamSerialNumber) != assignedSerialNums_.cend()) {
        streamSerialNumber = lfsrNext(streamSerialNumber);
    }

    return newLogicalStream(streamSerialNumber).value();
}

std::optional<OggLogicalStreamOut> OggPhysicalStreamOut:: newLogicalStream(const uint32_t streamSerialNumber) {
    if (assignedSerialNums_.find(streamSerialNumber) != assignedSerialNums_.cend()) {
        return std::optional<OggLogicalStreamOut>{};
    }
    assignedSerialNums_.insert(streamSerialNumber);
    return std::optional<OggLogicalStreamOut>{ OggLogicalStreamOut{ *this, streamSerialNumber } };
}
