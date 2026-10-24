// ======================================================================
// \title  ImageProtocolParser.hpp
// \brief  hpp file for ImageProtocolParser class
// ======================================================================

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Components {

//! Parses the camera image protocol: <IMG_START><SIZE>[u32 LE]</SIZE>[image data]
//!
//! Also recognizes the PONG reply to a ping. The parser owns the small protocol buffer and the transfer progress
//! (bytes received, expected size, progress milestone). The component owns the files: it opens the image file when
//! a header is found, writes the chunks the parser plans, and finalizes the image.
//!
//! Some behavior is surprising and kept as is, because the camera and the component were tuned around it:
//! - A transfer is complete when a chunk arrives and the byte count already reached the expected size, so a chunk
//!   that brings the count exactly to the expected size does not finish the image. The next chunk does.
//! - At most one progress milestone is reported per chunk.
//! - A PONG is not consumed from the buffer, so it is reported again until the buffer is trimmed.
//! - A header with bad tags is not discarded, it stays at the start of the buffer until the buffer is trimmed.
class ImageProtocolParser {
  public:
    // ----------------------------------------------------------------------
    // Public constants and types
    // ----------------------------------------------------------------------

    //! Size of the protocol buffer in bytes
    static constexpr std::uint32_t PROTOCOL_BUFFER_SIZE = 128;

    //! Size of a complete header in bytes: <IMG_START><SIZE>xxxx</SIZE>
    static constexpr std::uint32_t HEADER_SIZE = 28;

    //! What a chunk of protocol data contained
    enum class Found {
        NOTHING,       //!< No complete header and no PONG
        PONG,          //!< The camera answered a ping
        IMAGE_HEADER,  //!< A valid header, a transfer has started
    };

    //! Result of feeding protocol data
    struct Result {
        Found found;              //!< What was found
        std::uint32_t imageSize;  //!< The announced image size in bytes, only for IMAGE_HEADER
    };

    //! What to do with a chunk of image data
    struct ChunkPlan {
        bool complete;          //!< The image is complete, finalize it instead of writing the chunk
        std::uint32_t toWrite;  //!< Bytes of the chunk to write to the file, never past the expected size
    };

    //! Image data that arrived in the same chunk as the header
    struct Chunk {
        const std::uint8_t* data;  //!< Valid until the next call that changes the buffer
        std::uint32_t size;        //!< Bytes to write, never past the expected size
        bool present;              //!< Whether any bytes followed the header, even if none are to be written
    };

    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImageProtocolParser object
    ImageProtocolParser();

    //! Destroy ImageProtocolParser object
    ~ImageProtocolParser();

    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Feed data received while no image is being received
    //!
    //! Old data is dropped when the buffer is nearly full. On IMAGE_HEADER the transfer has started, and the
    //! header is still in the buffer until the caller calls takeBufferedImageData() or rejectHeader().
    Result feed(const std::uint8_t* data,  //!< The received data
                std::uint32_t size         //!< The number of bytes
    );

    //! Feed the data that followed the end of an image
    //!
    //! Unlike feed(), the data is dropped, not trimmed to fit, if it does not fit the buffer.
    Result feedAfterImage(const std::uint8_t* data,  //!< The data after the image
                          std::uint32_t size         //!< The number of bytes
    );

    //! Reject the header found by feed() because the file could not be opened
    //!
    //! Ends the transfer and clears the buffer.
    void rejectHeader();

    //! Remove the header from the buffer and get the image data that followed it
    //!
    //! Call after the file was opened. The caller writes the chunk, calls onChunkWritten(), then clearBuffer().
    Chunk takeBufferedImageData();

    //! Plan what to do with a chunk of image data of the given size
    ChunkPlan planChunk(std::uint32_t chunkSize  //!< The size of the received chunk
    ) const;

    //! Record bytes written to the file
    //!
    //! Returns the progress milestone (25, 50 or 75) crossed by this write, or 0 if none. At most one milestone
    //! is returned per call.
    std::uint8_t onChunkWritten(std::uint32_t bytes  //!< The number of bytes written
    );

    //! Record bytes written to the file without reporting progress
    void onBufferedDataWritten(std::uint32_t bytes  //!< The number of bytes written
    );

    //! End the transfer after the image was finalized
    void finishTransfer();

    //! End the transfer after an error and clear the buffer
    void abortTransfer();

    //! Clear the protocol buffer
    void clearBuffer();

    //! Whether the received bytes have reached the expected size
    bool isComplete() const;

    //! Whether an image is being received
    bool isReceiving() const;

    //! Get the number of image bytes received so far
    std::uint32_t bytesReceived() const;

    //! Get the announced size of the image being received
    std::uint32_t expectedSize() const;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Append data to the buffer, returns false and leaves the buffer unchanged if it does not fit
    bool accumulate(const std::uint8_t* data, std::uint32_t size);

    //! Look for a header or a PONG in the buffer
    Result process();

    //! Keep only the last bytes of the buffer, in case a header is split across chunks
    void keepLast(std::uint32_t count);

    //! Find the offset of a text in the buffer, or -1 if it is not there
    std::int32_t find(const char* text, std::uint32_t length) const;

    //! Whether the buffer holds the text at the offset
    bool matches(std::uint32_t offset, const char* text, std::uint32_t length) const;

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    std::array<std::uint8_t, PROTOCOL_BUFFER_SIZE> m_buffer;  //!< Commands and headers
    std::uint32_t m_bufferSize;                               //!< Bytes used in the buffer
    bool m_receiving;                                         //!< Whether an image is being received
    std::uint32_t m_bytesReceived;                            //!< Image bytes written so far
    std::uint32_t m_expectedSize;                             //!< Image size announced by the header
    std::uint8_t m_lastMilestone;                             //!< Last progress milestone reported (0, 25, 50, 75)
};

}  // namespace Components
