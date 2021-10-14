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
#include <functional>

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

    /**
    * Read-only representation of a single page of an Ogg stream.
    */
    class OggPage {
    public:

        /**
        * Helper type to construct an OggPage.
        * This struct contains the same members as OggPage, but is writable.
        */
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

        // True if this page's data belongs to an already started packet.
        const bool isContinuedPacket;

        // True if this page is the first page of a logical stream.
        const bool isFirstPage;

        // True if this page is the last page of a logical stream.
        const bool isLastPage;

        // Granule position. The interpretation of this value is subject of 
        // the decoder for the logical stream.
        const int64_t granulePosition;

        // Serial number of the logical stream that this page belongs to.
        const uint32_t streamSerialNumber;

        // Sequence number of this page. Within a logical stream, sequence numbers must
        // be continuous and ascending.
        const uint32_t pageSequenceNumber;

        // CRC32 checksum of this page.
        const uint32_t pageChecksum;

        // Length of the payload contained in this page.
        const std::size_t dataSize;

        // Payload of this page.
        const std::unique_ptr<const uint8_t[]> data;

        /**
        * Constructs an OggPage from a OggPage::Params object.
        * 
        * @param params The object containing the values for this page's members.
        */
        explicit OggPage(Params&& params);

        OggPage(const OggPage& other) = delete;
        OggPage& operator=(const OggPage& other) = delete;

        OggPage(OggPage&& other) = default;
        OggPage& operator=(OggPage&& other) = default;

    };

    class OggPhysicalStreamIn;

    /**
    * Represents a logical input stream inside of an OggPhysicalStreamIn.
    */
    class OggLogicalStreamIn {
    public:
        /**
        * Meta information about the data passed to DataCallback::onDataAvailable().
        */
        struct MetaData {
            // Current granule position. The meaning of this field depends on the content of the
            // stream (e.g. sample number for Ogg Vorbis)
            const int64_t granulePosition;

            // If there were any pages missing from the logical stream, this field indicates how many
            // pages were skipped since the last callback.
            const unsigned int numSkippedPages;

            // True if the current call marks the beginning of the logical stream, false otherwise.
            const bool isFirstData;

            // True if the data is a continuation of the previous packet, false otherwise.
            // Packets can be arbitrarily large and may need to be split across multiple pages.
            const bool isContinuedPacket;

            // True if this is the last call to the callback. 
            const bool isClosing;
        };

        class DataCallback {
        public:
            /**
            * Called when new data is available for the logical stream.
            * 
            * @param data Pointer to the raw data.
            * @param size Size of data.
            * @param meta Meta information about the data.
            */
            virtual void onDataAvailable(const uint8_t* const data, const std::size_t size, const MetaData meta) = 0;
        };

    private:
        std::vector<std::shared_ptr<DataCallback>> dataCallbacks_;
        int64_t granulePosition_;
        uint32_t streamSerialNumber_;
        uint32_t pageSequenceNumber_;
        bool isOpen_;

        explicit OggLogicalStreamIn(uint32_t streamSerialNumber);

        void processPage(const OggPage& page);
    public:

        OggLogicalStreamIn(const OggLogicalStreamIn& other) = delete;
        OggLogicalStreamIn& operator=(const OggLogicalStreamIn& other) = delete;

        OggLogicalStreamIn(OggLogicalStreamIn&& other) = default;
        OggLogicalStreamIn& operator=(OggLogicalStreamIn && other) = default;

        /**
        * Adds a callback to this OggLogicalStreamIn. The callback will be called when the stream
        * receives new data.
        * 
        * @param callback The callback.
        */
        void addDataCallback(const std::shared_ptr<DataCallback> callback);
        
        /**
        * Removes a callback from this OggLogicalStreamIn. If the callback is not found, this method 
        * does nothing.
        * 
        * @param callback Reference to the callback to be removed.
        */
        void removeDataCallback(const std::shared_ptr<DataCallback>& callback);

        friend OggPhysicalStreamIn;
    };

    /**
    * Represents a physical Ogg stream. A physical stream can contain one or more 
    * logical streams (OggLogicalStreamIn).
    */
    class OggPhysicalStreamIn {
    public:

        /**
        * Callback to be called when a new logical stream appears in the physical stream
        * (i.e. when a stream serial number appears for the first time).
        */
        class NewStreamCallback {
        public:
            virtual void onNewStream(OggLogicalStreamIn& stream) = 0;
        };

    private:
        /**
        * Abstract wrapper around some input method.
        */
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
        std::vector<std::shared_ptr<NewStreamCallback>> newStreamCallbacks_;
        std::unordered_map<uint32_t, OggLogicalStreamIn> logicalStreams_;

        /**
        * Reads a page from the physical stream. The stream is expected to be
        * right after the capture pattern 'OggS'.
        */
        OggPage readPage();

        /**
        * Advances the underlying stream to after the next occurance of the 
        * capture patter 'OggS'.
        */
        void resync();

    public:
        /**
        * Constructs an OggPhysicalStreamIn that reads from a basic_istream.
        * The basic_istream object must be valid for at least as long as the OggPhysicalStreamIn
        * object exists.
        * 
        * @param in Reference to the input stream to be used as a source.
        */
        explicit OggPhysicalStreamIn(std::basic_istream<uint8_t>& in);

        /**
        * Constructs an OggPhysicalStreamIn that reads from a file. If the 
        * input comes from a file, this is faster than using std::ifstream.
        * 
        * @param file FILE handle to be used as a source for the stream.
        */
        explicit OggPhysicalStreamIn(FILE* file);

        OggPhysicalStreamIn(const OggPhysicalStreamIn& other) = delete;
        OggPhysicalStreamIn& operator=(const OggPhysicalStreamIn& other) = delete;

        /**
        * Adds a NewStreamCallback to this OggPhysicalStreamIn.
        * 
        * @param callback The callback.
        */
        void addNewStreamCallback(const std::shared_ptr<NewStreamCallback> callback);

        /**
        * Removes a NewStreamCallback from this OggPhysicalStreamIn. If the callback
        * does not exist, this method does nothing.
        * 
        * @param callback The callback to remove.
        */
        void removeNewStreamCallback(const std::shared_ptr<NewStreamCallback>& callback);

        /**
        * Initiates processing of this OggPhysicalStreamIn. While this method runs, the
        * NewStreamCallbacks and DataCallbacks are called accordingly.
        */
        void process();
        
    };

    class OggPhysicalStreamOut;

    /**
    * Represents a logical output stream. Objects of this type are obtained by calling
    * OggPhysicalStreamOut.newLogicalStream() and are thereby associated with a specific
    * physical stream. OggLogicalStreamOut objects must not outlive their associated
    * physical stream.
    */
    class OggLogicalStreamOut {
        OggPhysicalStreamOut& sink_;
        uint32_t streamSerialNumber_;
        uint32_t pageSequenceNumber_;
        bool isPacketOpen_;
        bool isStreamOpen_;
        bool isFirstWrite_;

        OggLogicalStreamOut(OggPhysicalStreamOut& sink, const uint32_t streamSerialNumber);

    public:
        OggLogicalStreamOut(const OggLogicalStreamOut& other) = delete;
        OggLogicalStreamOut& operator=(const OggLogicalStreamOut& other) = delete;

        OggLogicalStreamOut(OggLogicalStreamOut&& other) noexcept;
        OggLogicalStreamOut& operator=(OggLogicalStreamOut&& other) = delete;

        void writePage(
            const uint8_t* const data,
            const unsigned int size,
            const int64_t granulePosition,
            const bool closePacket,
            const bool closeStream);

        /**
        * Writes data to this logical stream. The data is transparently transformed into pages.
        * 
        * @param data The data to be written.
        * @param size Size of data.
        * @param granulePosition Value for the granulePosition field.
        * @param closePacket Whether to close the current packet after the data has been written.
        * @param closeStream Whether to close the logical stream after the data has been written.
        */
        void write(
            const uint8_t* const data, 
            const unsigned int size,
            const int64_t granulePosition,
            const bool closePacket = true,
            const bool closeStream = false);

        friend OggPhysicalStreamOut;
    };

    /**
    * Representation of a physical output stream. Objects of this class coordinate one or more
    * logical streams and write their data to an output.
    */
    class OggPhysicalStreamOut {
        /**
        * Abstract wrapper around some output method.
        */
        class Output {
        public:
            virtual ~Output() = default;

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
        /**
        * Constructs an OggPhysicalStreamOut that writes to a file. If data is to be written to
        * a file, this is faster than useing std::ifstream.
        * 
        * @param file The FILE handle to write to.
        */
        explicit OggPhysicalStreamOut(FILE* file);

        /**
        * Constructs an OggPhysicalStreamOut that writes to a basic_ostream. The ostream 
        * must be valid for at least as long as the OggPhysicalStreamOut object exists.
        * 
        * @param out The basic_ostream to write to.
        */
        explicit OggPhysicalStreamOut(std::basic_ostream<uint8_t>& out);

        /**
        * Obtains a new OggLogicalStreamOut which is associated with this physical stream.
        * Its stream serial number is random.
        */
        OggLogicalStreamOut newLogicalStream();

        /**
        * Obtains a new OggLogicalStreamOut which is associated with this physical stream and
        * with the given stream serial number. If there already exists a stream with
        * that serial number, an empty std::optional is returned.
        * 
        * @param streamSerialNumber The desired serial number.
        */
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