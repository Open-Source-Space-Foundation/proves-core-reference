// ======================================================================
// \title  FlashWorker.hpp
// \author starchmd
// \brief  hpp file for FlashWorker component implementation class
// ======================================================================

#ifndef Update_FlashWorker_HPP
#define Update_FlashWorker_HPP
#include "FprimeZephyrReference/Components/FlashWorker/FlashWorkerComponentAc.hpp"
#include "Os/File.hpp"
#include <zephyr/dfu/flash_img.h>
namespace Components {

class FlashWorker final : public FlashWorkerComponentBase {
  public:
    constexpr static U8 REGION_NUMBER = 2;  // 0: bootloader, 1: slot0, **2: slot1**
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
