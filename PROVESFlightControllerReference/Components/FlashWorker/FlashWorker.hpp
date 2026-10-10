// ======================================================================
// \title  FlashWorker.hpp
// \author starchmd
// \brief  hpp file for FlashWorker component implementation class
// ======================================================================

#ifndef Update_FlashWorker_HPP
#define Update_FlashWorker_HPP
#include "Os/File.hpp"
#include "PROVESFlightControllerReference/Components/FlashWorker/FlashWorkerComponentAc.hpp"
#include <zephyr/devicetree.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/storage/flash_map.h>
namespace Components {

class FlashWorker final : public FlashWorkerComponentBase {
  public:
    //! MCUboot secondary slot: where an uploaded image is staged before the bootloader swaps it in.
    //!
    //! Resolved from the devicetree label, never hardcoded. Zephyr hands out flash-area IDs in
    //! devicetree dependency-ordinal order, so adding a partition anywhere in the DT renumbers every
    //! area. A hardcoded ID silently starts pointing at a different partition -- and erasing the
    //! wrong one here wipes the running firmware.
    constexpr static U8 REGION_NUMBER = PARTITION_ID(slot1_partition);

    //! Guard the failure above: the staging region must never be the slot we are executing from.
    static_assert(PARTITION_OFFSET(slot1_partition) != DT_REG_ADDR(DT_CHOSEN(zephyr_code_partition)),
                  "FlashWorker update region overlaps the running code partition");
    enum Step { IDLE, PREPARE, UPDATE };
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FlashWorker object
    FlashWorker(const char* const compName  //!< The component name
    );

    //! Destroy FlashWorker object
    ~FlashWorker();

  private:
    Update::UpdateStatus writeImage(const Fw::StringBase& file_name, Os::File& image_file, U32 crc32);

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

  private:
    Step m_last_successful;
    U8 m_data[CONFIG_IMG_BLOCK_BUF_SIZE];
    struct flash_img_context m_flash_context;
};

}  // namespace Components

#endif
