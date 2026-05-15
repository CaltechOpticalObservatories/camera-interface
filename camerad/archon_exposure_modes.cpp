/**
 * @file     archon_exposure_modes.h
 * @brief    implements Archon-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"
#include "archon_interface.h"

#include <cstdlib>

namespace Camera {

  /***** Camera::ExposureModeSingle *******************************************/
  /**
   * @brief  implementation of Archon-specific expose for Single
   *
   */
  long ExposureModeSingle::expose() {
    const std::string function("Camera::ExposureModeSingle::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }
  /***** Camera::ExposureModeSingle ******************************************/


  /***** Camera::ExposureModeSingle::image_acquisition_thread *****************/
  /**
   * @brief      producer thread for ExposureMode Single
   * @details    Spawned by Camera::ArchonInterface::do_expose()
   *
   */
  void ExposureModeSingle::image_acquisition_thread() {
    const std::string function("Camera::ExposureModeSingle::image_acquisition_thread");
    char message[256];

    logwrite(function, "");

    auto camera_info = &this->interface->camera_info;

    // record system time when exposure starts (YYYY-MM-DDTHH:MM:SS.sss)
    camera_info->start_time = get_timestamp();

/*****
 *  // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
 *  this->interface->set_fitstime(camera_info->start_time);
 *
 *  // assemble the FITS filename
 *  get_fitsname(camera_info->fits_name);
 *
 *  // add filename to system keys database
 *  this->add_filename_key();
*****/

/** why are there two of these? Can I get by with only this->interface->camera_info?
 *  // copy systemkeys databases into camera_info
 *  this->interface->camera_info.systemkeys.keydb = this->interface->systemkeys.keydb;
 **/

    this->interface->controller->get_frame_status();

    // initiate the exposure here
    //
    int nexp=1;
    if ( this->interface->controller->initiate_exposure(nexp) != NO_ERROR ) {
      logwrite(function, "could not initiate exposure");
      return;
    }
    logwrite(function, "exposure started");

    long error=NO_ERROR;

    uint64_t bufferbytes = (uint64_t)camera_info->image_data_bytes * camera_info->cubedepth;

    while (error==NO_ERROR && !this->interface->is_aborted() && nexp > 0) {
      // prepare an ImageBuffer object for the exposure
      auto imagebuffer = std::make_shared<ArchonImageBuffer>();

      try { imagebuffer->rawpixels = std::shared_ptr<char[]>(new char[bufferbytes]);
      }
      catch (const std::exception &e) {
        SNPRINTF(message, "memory allocation failed: %s", e.what());
        logwrite(function, "ERROR "+std::string(message));
        error=ERROR;
        break;
      }

      // wait for frame readout into Archon buffer
      if ( (error=this->interface->controller->wait_for_readout()) == ERROR ) break;

      // read frame from Archon into memory pointed to by p_imagebuffer
      char* p_imagebuffer = imagebuffer->rawpixels.get();
      this->interface->controller->read_frame(ArchonController::FRAME_IMAGE, p_imagebuffer);

      // frame metadata
      auto index = this->interface->controller->frameinfo.index.load();
      imagebuffer->bufframen_slice.push_back( this->interface->controller->frameinfo.bufframen[index] );
      imagebuffer->buftimestamp_slice.push_back( this->interface->controller->frameinfo.buftimestamp[index] );

      // push frame into queue
      {
      std::lock_guard<std::mutex> lock(this->queue_mutex);
      this->imagebuf_queue.push(imagebuffer);
      this->queue_cv.notify_one();
      }
      nexp--;
    }  // end loop over number of frames

    logwrite(function, "complete");
  }
  /***** Camera::ExposureModeSingle::image_acquisition_thread *****************/


  /***** Camera::ExposureModeSingle::image_processing_thread ******************/
  /**
   * @brief  Consumer thread: pop each frame off the queue and fan out to
   *         every configured FrameOutput on the interface
   */
  void ExposureModeSingle::image_processing_thread() {
    const std::string function("Camera::ExposureModeSingle::image_processing_thread");
    logwrite(function, "enter");

    auto* camera_info = &this->interface->camera_info;
    auto* controller  = this->interface->controller;
    auto* mode        = &controller->modemap[controller->selectedmode];
    const size_t   bufferbytes = static_cast<size_t>(camera_info->image_data_bytes) * camera_info->cubedepth;
    const uint32_t bpp         = (mode->samplemode == 1) ? 4 : 2;
    const uint32_t width       = static_cast<uint32_t>(mode->geometry.pixelcount);
    const uint32_t height      = static_cast<uint32_t>(mode->geometry.linecount);

    while (!this->interface->is_aborted()) {
      std::shared_ptr<ArchonImageBuffer> buf;
      {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        this->queue_cv.wait(lock, [this] {
            return !this->imagebuf_queue.empty() || this->is_producer_finished || this->interface->is_aborted();
            });
        if (this->interface->is_aborted()) break;
        if (this->imagebuf_queue.empty()) {
          if (this->is_producer_finished) {
            logwrite(function, "queue empty and producer finished");
            break;
          }
          continue;
        }
        buf = this->imagebuf_queue.front();
        this->imagebuf_queue.pop();
      }

      Camera::FrameMetadata meta;
      meta.frame_number    = buf->bufframen_slice.empty()    ? 0 : static_cast<uint64_t>(buf->bufframen_slice[0]);
      meta.timestamp       = buf->buftimestamp_slice.empty() ? 0 : buf->buftimestamp_slice[0];
      meta.width           = width;
      meta.height          = height;
      meta.bytes_per_pixel = bpp;
      this->interface->dispatch_frame(buf->rawpixels.get(), bufferbytes, meta);
    }

    logwrite(function, "exit");
  }
  /***** Camera::ExposureModeSingle::image_processing_thread ******************/


  /***** Camera::ExposureModeSingle::process *********************************/
  /**
   * @brief      image process a Single image
   * @param[in]  imagebuffer  reference to Archon Image Buffer object
   *
   */
  void ExposureModeSingle::process_image(std::shared_ptr<ArchonImageBuffer> &imagebuffer) {
  }
  /***** Camera::ExposureModeSingle::process *********************************/


  /***** Camera::ExposureModeRaw::expose *************************************/
  /**
   * @brief  implementation of Archon-specific expose for Raw
   *
   */
  long ExposureModeRaw::expose() {
    logwrite("Camera::ExposureModeRaw::expose","");
    return 0;
  }
  /***** Camera::ExposureModeRaw::expose *************************************/


  /***** Camera::ExposureModeRXRV ********************************************/
  /**
   * @brief  implementation of Archon-specific expose for RXR-Video
   *
   */
  long ExposureModeRXRV::expose() {
    const std::string function("Camera::ExposureModeRXRV::expose");

    size_t sz=100;

    // Two each of signal and reset buffers, current and previous, since
    // we need to pair the reset from the previous frame with signal from
    // the current frame. These will hold deinterlaced frames.
    //
    std::vector<std::vector<uint16_t>> sigbuf(2, std::vector<uint16_t>(sz));
    std::vector<std::vector<uint16_t>> resbuf(2, std::vector<uint16_t>(sz));

    // allocate memory for frame buffer read from Archon
    interface->allocate_framebuf(sz);

    // create an appropriate deinterlacer object
    try { processor = make_image_processor("rxrv");
    }
    catch(const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    // read first frame pair into my frame buffer
    char* buffer=new char[1024]{};  // TODO temporary, for compilation only
    this->interface->controller->read_frame(ArchonController::FRAME_IMAGE, buffer);

    // process (deinterlace) first frame pair
    processor->deinterlacer()->deinterlace(interface->get_framebuf(), sigbuf[0].data(), resbuf[0].data());

    // sample calls to other processor functions
    uint16_t a, b;
    int16_t c;
    processor->subtractor()->subtract(&a, &b, &c);
    processor->coadder()->coadd(&a, &b);

    // show contents
    std::stringstream message;
    message << "sig:";
    for (int i=0; i<10; i++) message << " " << sigbuf[0][i];
    logwrite(function, message.str());
    message.str(""); message << "res:";
    for (int i=0; i<10; i++) message << " " << resbuf[0][i];
    logwrite(function, message.str());

    // loop:
    // subsequent frame pairs, read, deinterlace, write

    delete [] buffer;

    return NO_ERROR;
  }
  /***** Camera::ExposureModeRXRV ********************************************/

}
