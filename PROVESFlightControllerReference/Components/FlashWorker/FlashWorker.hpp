// ======================================================================
// \title  FlashWorker.hpp
// \author starchmd
// \brief  hpp file for FlashWorker component implementation class
// ======================================================================

#ifndef Update_FlashWorker_HPP
#define Update_FlashWorker_HPP
#include "Os/File.hpp"
#include "PROVESFlightControllerReference/Components/FlashWorker/FlashWorkerComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/FlashWorker/LzssDecoder.hpp"
#include "PROVESFlightControllerReference/Components/FlashWorker/PatchApplier.hpp"
#include "PROVESFlightControllerReference/Components/FlashWorker/SegmentPlan.hpp"
#include "PROVESFlightControllerReference/Components/FlashWorker/UpdateSequencer.hpp"
#include <zephyr/dfu/flash_img.h>
#include <zephyr/storage/flash_map.h>
namespace Components {

class FlashWorker final : public FlashWorkerComponentBase {
  public:
    //! Flash area holding the MCUBoot staging slot that updates are written into.
    //!
    //! Derived from the device tree rather than hard coded: fixed partition IDs follow the
    //! declaration order of the partitions node, so a literal would silently point at the wrong
    //! region if a partition were ever added above slot1_partition.
    constexpr static U8 REGION_NUMBER = FIXED_PARTITION_ID(slot1_partition);
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FlashWorker object
    FlashWorker(const char* const compName  //!< The component name
    );

    //! Destroy FlashWorker object
    ~FlashWorker();

  private:
    //! Stream an image file into the staging slot, reporting what went wrong if anything did
    UpdateSequencer::WriteOutcome writeImage(const Fw::StringBase& file_name, Os::File& image_file, U32 crc32);

    //! Convert a sequencer status into the autocoded status reported over the update ports
    static Update::UpdateStatus toUpdateStatus(UpdateSequencer::Status status);

    //! Convert a sequencer stage into the autocoded stage reported as telemetry
    static Components::FlashUpdateStage toUpdateStage(UpdateSequencer::Stage stage);

    //! Report the stage of the sequence, along with the status of the operation that produced it
    void reportStage(Components::FlashUpdateStage stage, Update::UpdateStatus status);

    //! Report write progress, emitting an event only when the configured step has been passed
    void reportProgress(U32 written, U32 total);

    //! Sources and sink used when applying a delta patch
    class PatchIo;

    //! Compressed patch input and decoded output, used when a patch carries a codec
    class PatchSource;
    class PatchSink;

    //! Decompress a patch payload into a scratch file, leaving the apply to operate on plain
    //! streams. Returns true on success; the caller reports the failure.
    bool decompressPatch(const Fw::StringBase& patch, const char* scratch_path, U32 expected_size);

    //! Handler implementation for command ASSEMBLE_IMAGE
    void ASSEMBLE_IMAGE_cmdHandler(FwOpcodeType opCode,                  //!< The opcode
                                   U32 cmdSeq,                           //!< The command sequence number
                                   const Fw::CmdStringArg& prefix,       //!< Segment file name prefix
                                   U16 segments,                         //!< Number of segments to join
                                   const Fw::CmdStringArg& destination,  //!< Image file to write
                                   U32 crc32                             //!< Expected CRC32 of the image
                                   ) override;

    //! Handler implementation for command APPLY_PATCH
    void APPLY_PATCH_cmdHandler(FwOpcodeType opCode,                  //!< The opcode
                                U32 cmdSeq,                           //!< The command sequence number
                                const Fw::CmdStringArg& patch,        //!< Patch file to apply
                                const Fw::CmdStringArg& destination,  //!< Image file to write
                                U32 crc32                             //!< Expected CRC32 of the image
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for confirmImage
    //!
    //! Confirm that the currently running image is good
    Update::UpdateStatus confirmImage_handler(FwIndexType portNum  //!< The port number
                                              ) override;

    //! Handler implementation for nextBoot
    //!
    //! Set the next boot image and mode
    Update::UpdateStatus nextBoot_handler(FwIndexType portNum,  //!< The port number
                                          const Update::NextBootMode& mode) override;

    //! Handler implementation for prepareImage
    void prepareImage_handler(FwIndexType portNum  //!< The port number
                              ) override;

    //! Handler implementation for updateImage
    void updateImage_handler(FwIndexType portNum,         //!< The port number
                             const Fw::StringBase& file,  //!< File to read image from
                             U32 crc32                    //!< Expected CRC32 of the file used to verify file integrity
                             ) override;

    //! Handler implementation for run
    //!
    //! Ages a test-booted image toward self-confirmation
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    UpdateSequencer m_sequencer;
    U32 m_pending_confirm_seconds;  //!< Seconds the running image has been up while unconfirmed
    U8 m_last_reported_percent;     //!< Percent at the most recent progress event, for step throttling
    U8 m_data[CONFIG_IMG_BLOCK_BUF_SIZE];
    U8 m_window[LzssDecoder::WINDOW_SIZE];  //!< History buffer for decoding a compressed patch
    struct flash_img_context m_flash_context;
};

}  // namespace Components

#endif
