/**
 * @file    fits.h
 * @brief   fits interface functions header file
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include <CCfits/CCfits>
#include <fstream>         //!< for ofstream
#include "common.h"
#include <thread>
#include <atomic>

template <class T>
class FITS_file {
  private:
    std::mutex fits_mutex;                          //!< used to block writing_file semaphore in multiple threads
    std::unique_ptr<CCfits::FITS> pFits;            //!< pointer to FITS data container
    std::string fits_name;                          //!< name for FITS file
    int num_frames;
    std::atomic<bool> writing_file;                 //!< semaphore indicates file is being written
    std::atomic<int> threadcount;                   //!< keep track of number of write_image_thread threads

  public:
    FITS_file() {
      this->threadcount = 0;
      this->num_frames = 0;
      this->writing_file = false;
    };

    ~FITS_file() {
    };

    /**************** FITS_file::open_file ************************************/
    /**
     * @fn     open_files
     * @brief  
     * @param  
     * @return 
     *
     */
    long open_file(Archon::Information & info) {
      const char* function = "FITS_file::open_file";

      const std::lock_guard<std::mutex> lock(this->fits_mutex);

      // Check that we can write the file, because CCFits will crash if it cannot
      //
      std::ofstream checkfile ( info.image_name.c_str() );
      if ( checkfile.is_open() ) {
        checkfile.close();
        std::remove( info.image_name.c_str() );
      }
      else {
        Logf("(%s) error unable to create file %s\n", function, info.image_name.c_str());
        return(ERROR);
      }

      try {
        // Allocate the FITS file container, which holds the information used by CCfits to write a file
        //
        this->pFits.reset( new CCfits::FITS(info.image_name, info.bitpix, info.naxis, info.axes) );
      }
      catch (CCfits::FITS::CantCreate){
        Logf("(%s) error: unable to open FITS file %s\n", function, info.image_name.c_str());
        return(ERROR);
      }
      catch (...) {
        Logf("(%s) unknown error\n", function);
        return(ERROR);
      }

      Logf("(%s) opened file %s for FITS write\n", function, info.image_name.c_str());

      return (0);
    }
    /**************** FITS_file::open_file ************************************/


    /**************** FITS_file::close_file ***********************************/
    /**
     * @fn     open_files
     * @brief  
     * @param  
     * @return 
     *
     */
    long close_file() {
      const char* function = "FITS_file::close_file";

      Common::Utilities util;

      // Add a header keyword for the time the file was written (right now!)
      //
      this->pFits->pHDU().addKey("DATE", util.get_time_string(), "FITS file write date");

      // Write the checksum
      //
      this->pFits->pHDU().writeChecksum();

      // Deallocate the CCfits object and close the FITS file
      //
      this->pFits->destroy();

      // Log that the file closed successfully
      //
      Logf("(%s) successfully closed FITS file %s\n", function, this->fits_name.c_str());;

      return(NO_ERROR);
    }
    /**************** FITS_file::close_file ***********************************/


    /**************** FITS_file::write_image **********************************/
    /**
     * @fn     write_image
     * @brief  
     * @param  
     * @return 
     *
     */
    long write_image(T* data, Archon::Information& info) {
      const char* function = "FITS::write_image";

      if (info.data_cube == true) {
        Logf("(%s) error data cube not supported\n", function);
        return (ERROR);
      }
      else {
        std::valarray<T> array(info.image_size);
        for (unsigned long i = 0; i < info.image_size; i++){
          array[i] = data[i];
        }
        this->threadcount++;                                   // increment threadcount for each thread spawned
        std::thread([&]() {
          this->write_image_thread(std::ref(array), std::ref(info), this);
          std::lock_guard<std::mutex> lock(this->fits_mutex);  // lock and
          this->threadcount--;                                 // decrement threadcount
        }).detach();
        while (this->threadcount > 0) usleep(1000);            // wait for all threads to complete
      }

      Logf("(%s) complete\n", function);
      return(NO_ERROR);
    }
    /**************** FITS_file::write_image **********************************/


    /**************** FITS_file::write_image_thread ***************************/
    /**
     * @fn     write_image_thread
     * @brief  
     * @param  
     * @return 
     *
     */
    void write_image_thread(std::valarray<T> &data, Archon::Information &info, FITS_file<T> *self) {
      const char* function = "FITS_file::write_image_thread";

      // This makes the thread wait while another thread is writing images. This
      // function is really for single image writing, it's here just in case.
      //
      while (self->writing_file == true){
        usleep(1000);
      }

      // Set the FITS system to verbose mode so it writes error messages
      //
      CCfits::FITS::setVerboseMode(true);

      // Open the FITS file
      //
      if (self->open_file(info) != NO_ERROR){
        Logf("(%s) error failed to open FITS file %s\n", function, info.image_name.c_str());
        return;
      }

      // Lock the mutex and set the semaphore for file writing
      //
      const std::lock_guard<std::mutex> lock(self->fits_mutex);
      self->writing_file = true;

      // write the primary image into the FITS file
      //
      try {
        long fpixel = 1;       // start with the first pixel always
        self->pFits->pHDU().write(fpixel, info.image_size, data);
        self->pFits->flush();  // make sure the image is written to disk
      }
      catch (CCfits::FitsError& error){
        Logf("(%s) FITS file error thrown: %s\n", function, error.message().c_str());
        self->writing_file = false;
        return;
      }

      // Close the FITS file
      //
      if (self->close_file() != NO_ERROR){
        Logf("(%s) error failed to close FITS file properly: \n", function, self->fits_name.c_str());
        return;
      }
      self->writing_file = false;
    }
    /**************** FITS_file::write_image_thread ***************************/

};
