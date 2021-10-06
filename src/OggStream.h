#ifndef VORBIS_CPP_H
#define VORBIS_CPP_H

#include <istream>
#include <stdexcept>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>

namespace vcpp {
    class OggStreamError;
    class OggPage;
    class OggPacket;
    class OggPhysicalStream;

    class OggStreamError : public std::runtime_error {
    public:
        enum class Cause {
            UnexpectedEOF,
            BadChecksum,
            IOError,
            Other
        };

    private:
        const Cause cause;

    public:
        OggStreamError(const Cause cause, const std::string& what) : std::runtime_error(what), cause(cause) {}

        Cause getCause() const {
            return cause;
        }
    };

    class OggPacket {
    public:
        struct NewPageCallback;

    private:
        OggPhysicalStream& source;

        std::vector<NewPageCallback*> callbacks;

    public:
        struct NewPageCallback {
            virtual void onNewPage(const OggPage& page) = 0;
        };

        void addNewPageCallback(NewPageCallback& callback);
    };

    class OggPhysicalStream {
    public:
        struct NewStreamCallback;
        struct NewPacketCallback;

    private:
        std::basic_istream<uint8_t>& in;

        std::vector<NewStreamCallback*> newStreamCallbacks;

        std::unordered_map<uint32_t, std::vector<NewPacketCallback*>> newPacketCallbacks;

        OggPage readPage();

    public:
        struct NewStreamCallback {
            virtual void onNewStream(OggPacket& firstPacket) = 0;
        };

        struct NewPacketCallback {
            virtual void onNewPacket(OggPacket& packet) = 0;
        };

        explicit OggPhysicalStream(std::basic_istream<uint8_t>& in) : in(in) {}

        OggPhysicalStream(const OggPhysicalStream& other) = delete;
        OggPhysicalStream& operator=(const OggPhysicalStream& other) = delete;

        void addNewStreamCallback(NewStreamCallback& callback);

        void addNewPacketCallback(uint32_t streamSerialNumber, NewPacketCallback& callback);

        void process();
    };

    class OggPage {
        bool continuedPacket_;
        bool firstPage_;
        bool lastPage_;
        uint64_t granulePosition_;
        uint32_t streamSerialNumber_;
        uint32_t pageSequenceNumber_;
        uint32_t pageChecksum_;

        std::size_t dataSize_;
        std::unique_ptr<uint8_t> data_;

    public:
        explicit OggPage(std::size_t dataSize) : dataSize_(dataSize) {
            data_ = std::unique_ptr<uint8_t>(new uint8_t[dataSize]);
        }

        ~OggPage() = default;

        OggPage(const OggPage& other) = delete;
        OggPage& operator=(const OggPage& other) = delete;

        OggPage(OggPage&& other) = default;
        OggPage& operator=(OggPage&& other) = default;

    };

    class VorbisDetector : public OggPhysicalStream::NewStreamCallback, OggPacket::NewPageCallback {
    public:
        void onNewStream(OggPacket& firstPacket);

        void onNewPage(const OggPage& page);
    };
}

#endif