#include "OggStream.h"
#include <cstdint>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <memory>
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

using namespace vcpp;

class TestDataCallback : public OggLogicalStreamIn::DataCallback {
public:
    std::size_t numPackets;
    std::size_t bytesRead;
    bool isFirstPage;
    bool isClosed;

    TestDataCallback() : numPackets{ 0 }, bytesRead{ 0 }, isFirstPage{ true }, isClosed{ false } {}

    void onDataAvailable(const uint8_t* const data, const std::size_t size, OggLogicalStreamIn::MetaData meta) {
        RC_ASSERT(meta.numSkippedPages == 0);
        RC_ASSERT(isFirstPage == meta.isFirstData);
        RC_ASSERT_FALSE(isClosed);

        isFirstPage = false;

        if (!meta.isContinuedPacket) {
            numPackets++;
        }
        bytesRead += size;

        if (meta.isClosing) {
            isClosed = true;
        }
    }
};

template<typename C>
class TestNewStreamCallback : public OggPhysicalStreamIn::NewStreamCallback {
public:
    std::vector<std::shared_ptr<C>> dataCallbacks;

    TestNewStreamCallback() {}

    void onNewStream(OggLogicalStreamIn& stream) {
        dataCallbacks.emplace_back(std::make_shared<C>());
        stream.addDataCallback(dataCallbacks.back());
    }
};

RC_GTEST_PROP(TestOggStream, data_is_read_and_written_correctly,
    (const std::vector<uint32_t> packetSizes, const std::size_t numLogicalStreamsRaw)) {
    RC_PRE(packetSizes.size() > 0);
    RC_PRE(packetSizes.size() <= 20);
    RC_PRE(numLogicalStreamsRaw > 0);

    // Setup streams
    std::basic_stringstream<uint8_t> stream{};
    OggPhysicalStreamOut outPhysical{ stream };
    std::vector<OggLogicalStreamOut> logicalStreams;
    const std::size_t numLogicalStreams{ numLogicalStreamsRaw % 10 + 1 };
    for (std::size_t i{ 0 }; i < numLogicalStreams; i++) {
        logicalStreams.emplace_back(outPhysical.newLogicalStream());
    }

    // Limit packets sizes
    std::vector<uint32_t> sizesModulo(packetSizes.size());
    std::transform(packetSizes.cbegin(), packetSizes.cend(), sizesModulo.begin(),
        [&](uint32_t val) { return val % 100000; });

    // Allocate packet data buffer
    const uint32_t maxSize = *(std::max_element(sizesModulo.cbegin(), sizesModulo.cend()));
    const std::unique_ptr<uint8_t[]> data{ new uint8_t[maxSize] };

    // Write data
    std::vector<std::size_t> packetsWritten(numLogicalStreams);
    std::vector<std::size_t> bytesWritten(numLogicalStreams);
    std::size_t streamIndex{ 0 };
    for (std::size_t i{ 0 }; i < sizesModulo.size(); i++) {
        for (std::size_t j{ 0 }; j < sizesModulo[i]; j++) {
            data[j] = uint8_t(j & 0xff);
        }
        logicalStreams[streamIndex].write(data.get(), sizesModulo[i], 0, true, i + numLogicalStreams >= sizesModulo.size());
        packetsWritten[streamIndex]++;
        bytesWritten[streamIndex] += sizesModulo[i];
        streamIndex = (streamIndex + 1) % numLogicalStreams;
    }

    // Construct input stream
    OggPhysicalStreamIn inPhysical{ stream };

    // Read data
    const std::shared_ptr<TestNewStreamCallback<TestDataCallback>> callback{ std::make_shared<TestNewStreamCallback<TestDataCallback>>() };
    inPhysical.addNewStreamCallback(callback);
    inPhysical.process();

    // Check results
    for (std::size_t i{ 0 }; i < numLogicalStreams; i++) {
        if (packetsWritten[i] > 0) {
            const TestDataCallback& dc = *(callback->dataCallbacks[i]);
            RC_ASSERT(dc.numPackets == packetsWritten[i]);
            RC_ASSERT(dc.bytesRead == bytesWritten[i]);
            RC_ASSERT(dc.isClosed);
        }
    }
}
