#ifndef VORBIS_CPP_H
#define VORBIS_CPP_H

#include "util.h"

#include <istream>
#include <ostream>
#include <stdexcept>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>

namespace vcpp {
    class OggStreamError : public std::runtime_error {
    public:
        enum class Cause {
            UnexpectedEOF,
            LatePage,
            BadChecksum,
            IOError,
            StreamClosed,
            Other
        };

    private:
        const Cause cause_;

    public:
        OggStreamError(const Cause cause, const std::string& what) : std::runtime_error(what), cause_(cause) {}

        Cause getCause() const {
            return cause_;
        }
    };

    class OggPage {
    public:
        struct Params {
            uint8_t streamStructureVersion;
            bool isContinuedPacket;
            bool isFirstPage;
            bool isLastPage;
            int64_t granulePosition;
            uint32_t streamSerialNumber;
            uint32_t pageSequenceNumber;
            uint32_t pageChecksum;

            std::size_t dataSize;
            std::unique_ptr<uint8_t[]> data;

            inline Params(): 
                streamStructureVersion{ 0 },
                isContinuedPacket{ false },
                isFirstPage{ false },
                isLastPage{ false },
                granulePosition{ 0 },
                streamSerialNumber{ 0 },
                pageSequenceNumber{ 0 },
                pageChecksum{ 0 },
                dataSize{ 0 },
                data{ nullptr }
            {}

            Params(const Params& other) = delete;
            Params(Params&& other) = delete;
            Params& operator=(const Params& other) = delete;
            Params& operator=(Params&& other) = delete;
        };

        const bool isContinuedPacket;
        const bool isFirstPage;
        const bool isLastPage;
        const int64_t granulePosition;
        const uint32_t streamSerialNumber;
        const uint32_t pageSequenceNumber;
        const uint32_t pageChecksum;

        const std::size_t dataSize;
        const std::unique_ptr<const uint8_t[]> data;

        explicit OggPage(Params&& params);

        OggPage(const OggPage& other) = delete;
        OggPage& operator=(const OggPage& other) = delete;

        OggPage(OggPage&& other) = default;
        OggPage& operator=(OggPage&& other) = default;

    };

    class OggLogicalStreamIn {
    public:
        struct MetaData {
            const int64_t granulePosition;
            const unsigned int numSkippedPages;
            const bool isContinuedPacket;
            const bool isClosing;
        };

        struct DataCallback {
            virtual void onDataAvailable(const std::unique_ptr<const uint8_t[]>& data, const std::size_t size, MetaData meta) = 0;
        };

    private:
        std::vector<DataCallback*> dataCallbacks_;
        int64_t granulePosition_;
        uint32_t streamSerialNumber_;
        uint32_t pageSequenceNumber_;
        bool isOpen_;

    public:
        explicit OggLogicalStreamIn(uint32_t streamSerialNumber);

        void addDataCallback(DataCallback& callback);
        void removeDataCallback(const DataCallback& callback);
        void processPage(const OggPage& page);
    };

    class OggPhysicalStreamIn {
    public:
        struct NewStreamCallback {
            virtual void onNewStream(OggLogicalStreamIn& stream) = 0;
        };

    private:
        class Input {
        public:
            virtual ~Input() = default;

            virtual uint8_t read() = 0;
            virtual std::size_t read(uint8_t* const buffer, const std::size_t count) = 0;
            virtual bool eof() const = 0;
        };

        class FileInput : public Input {
            FILE* const file_;

        public:
            FileInput(FILE* file);

            uint8_t read() override;
            std::size_t read(uint8_t* const buffer, const std::size_t count) override;
            bool eof() const override;
        };

        class StreamInput : public Input {
            std::basic_istream<uint8_t>& in_;

        public:
            StreamInput(std::basic_istream<uint8_t>& in);

            uint8_t read() override;
            std::size_t read(uint8_t* const buffer, const std::size_t count) override;
            bool eof() const override;
        };

        const std::unique_ptr<Input> input_;
        std::vector<NewStreamCallback*> newStreamCallbacks_;
        std::unordered_map<uint32_t, OggLogicalStreamIn> logicalStreams_;

        OggPage readPage();
        void resync();

    public:
        explicit OggPhysicalStreamIn(std::basic_istream<uint8_t>& in);
        explicit OggPhysicalStreamIn(FILE* file);

        OggPhysicalStreamIn(const OggPhysicalStreamIn& other) = delete;
        OggPhysicalStreamIn& operator=(const OggPhysicalStreamIn& other) = delete;

        void addNewStreamCallback(NewStreamCallback& callback);
        void removeNewStreamCallback(const NewStreamCallback& callback);

        void process();
        
    };

    class OggPhysicalStreamOut;

    class OggLogicalStreamOut {
        OggPhysicalStreamOut& sink_;
        uint32_t streamSerialNumber_;
        uint32_t pageSequenceNumber_;
        bool isPacketOpen_;
        bool isStreamOpen_;
        bool isFirstWrite_;

        OggLogicalStreamOut(OggPhysicalStreamOut& sink, const uint32_t streamSerialNumber);

    public:

        void writePage(
            const uint8_t* const data,
            const unsigned int size,
            const int64_t granulePosition,
            const bool closePacket,
            const bool closeStream);

        void write(
            const uint8_t* const data, 
            const unsigned int size,
            const int64_t granulePosition,
            const bool closePacket = true,
            const bool closeStream = false);

        friend OggPhysicalStreamOut;
    };

    class OggPhysicalStreamOut {
        class Output {
        public:
            virtual void write(const uint8_t val) = 0;
            virtual void write(const uint8_t* const buffer, std::size_t count) = 0;
        };

        class FileOutput : public Output {
            FILE* file_;

        public:
            FileOutput(FILE* file);

            void write(const uint8_t val) override;
            void write(const uint8_t* const buffer, const std::size_t count) override;
        };

        class StreamOutput : public Output {
            std::basic_ostream<uint8_t>& out_;

        public:
            StreamOutput(std::basic_ostream<uint8_t>& out);

            void write(const uint8_t val) override;
            void write(const uint8_t* const buffer, const std::size_t count) override;
        };

        std::mutex writeLock;
        std::unique_ptr<Output> output_;
        std::set<uint32_t> assignedSerialNums_;

    public:
        explicit OggPhysicalStreamOut(FILE* file);
        explicit OggPhysicalStreamOut(std::basic_ostream<uint8_t> out);

        OggLogicalStreamOut newLogicalStream();
        std::optional<OggLogicalStreamOut> newLogicalStream(const uint32_t streamSerialNumber);

        friend void OggLogicalStreamOut::writePage(
            const uint8_t* const data, 
            const unsigned int size,
            const int64_t granulePosition,
            const bool closePacket,
            const bool closeStream);
    };
}

#endif