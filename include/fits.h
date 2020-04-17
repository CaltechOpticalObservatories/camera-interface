#ifndef ARCHONFITS_H
#define ARCHONFITS_H
/**
 * @file    fits.h
 * @brief   fits interface functions to CCFits
 * @details template class for FITS I/O operations using CCFits
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * This file includes the complete template class for FITS operations
 * using the CCFits library. If you're looking for the FITS keyword
 * database you're in the wrong place -- that's in common. This is just
 * FITS file operations.
 *
 */
#include <CCfits/CCfits>
#include <fstream>         //!< for ofstream
#include <thread>
#include <atomic>
#include "common.h"
#include "build_date.h"

template <class T>
class FITS_file {
  private:
    std::mutex fits_mutex;                          //!< used to block writing_file semaphore in multiple threads
    std::unique_ptr<CCfits::FITS> pFits;            //!< pointer to FITS data container
    std::atomic<bool> writing_file;                 //!< semaphore indicates file is being written
    std::atomic<int> threadcount;                   //!< keep track of number of write_image_thread threads

  public:
    FITS_file() {                                   //!< constructor
      this->threadcount = 0;
      this->writing_file = false;
    };

    ~FITS_file() {                                  //!< deconstructor
    };


    /**************** FITS_file::open_file ************************************/
    /**
     * @fn     open_file
     * @brief  opens a FITS file
     * @param  Information& info, reference to camera_info class
     * @return 
     *
     * This uses CCFits to create a FITS container, opens the file and writes
     * primary header data to it.
     *
     */
    long open_file(Archon::Information & info) {
      const char* function = "FITS_file::open_file";

      const std::lock_guard<std::mutex> lock(this->fits_mutex);

      // Check that we can write the file, because CCFits will crash if it cannot
      //
      std::ofstream checkfile ( info.fits_name.c_str() );
      if ( checkfile.is_open() ) {
        checkfile.close();
        std::remove( info.fits_name.c_str() );
      }
      else {
        Logf("(%s) error unable to create file %s\n", function, info.fits_name.c_str());
        return(ERROR);
      }

      try {
        // Allocate the FITS file container, which holds the information used by CCfits to write a file
        // and write the primary camera header information.
        //
        this->pFits.reset( new CCfits::FITS(info.fits_name, info.datatype, info.naxis, info.axes) );
        this->make_camera_header(info);

        // Iterate through the user-defined FITS keyword databases and add them to the primary header.
        //
        Common::FitsKeys::fits_key_t::iterator keyit;
        for (keyit  = info.userkeys.keydb.begin();
             keyit != info.userkeys.keydb.end();
             keyit++) {
          add_user_key(keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment);
        }
      }
      catch (CCfits::FITS::CantCreate){
        Logf("(%s) error: unable to open FITS file %s\n", function, info.fits_name.c_str());
        return(ERROR);
      }
      catch (...) {
        Logf("(%s) unknown error\n", function);
        return(ERROR);
      }

      Logf("(%s) opened file %s for FITS write\n", function, info.fits_name.c_str());

      return (0);
    }
    /**************** FITS_file::open_file ************************************/


    /**************** FITS_file::close_file ***********************************/
    /**
     * @fn     close_file
     * @brief  closes fits file
     * @param  none
     * @return none
     *
     * Before closing the file, DATE and CHECKSUM keywords are added.
     * Nothing called returns anything so this doesn't return anything.
     *
     */
    void close_file() {
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
    }
    /**************** FITS_file::close_file ***********************************/


    /**************** FITS_file::write_image **********************************/
    /**
     * @fn     write_image
     * @brief  spawn threads to write image data to FITS file on disk
     * @param  T* data, pointer to the data using template type T
     * @param  Information& into, reference to the fits_info class
     * @return 
     *
     * This function spawns a thread to write the image data to disk
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
        for (long i = 0; i < info.image_size; i++){
          array[i] = data[i];
        }
        // Use a lambda expression to properly spawn a thread without having to
        // make it static. Each thread gets a pointer to the current object this->
        // which must continue to exist until all of the threads terminate.
        // That is ensured by keeping threadcount, incremented for each thread
        // spawned and decremented on return, and not returning from this function
        // until threadcount is zero.
        //
        this->threadcount++;                                   // increment threadcount for each thread spawned
        std::thread([&]() {
          this->write_image_thread(std::ref(array), std::ref(info), this);
          std::lock_guard<std::mutex> lock(this->fits_mutex);  // lock and
          this->threadcount--;                                 // decrement threadcount
        }).detach();
        Logf("(%s) waiting for threads to complete\n", function);
        while (this->threadcount > 0) usleep(1000);            // wait for all threads to complete
      }

      Logf("(%s) complete\n", function);
      return(NO_ERROR);
    }
    /**************** FITS_file::write_image **********************************/


    /**************** FITS_file::write_image_thread ***************************/
    /**
     * @fn     write_image_thread
     * @brief  This is where the data are actually written
     * @param  
     * @return 
     *
     * This is the worker thread, to write the data using CCFits.
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
      // (internally mutex-protected)
      //
      if (self->open_file(info) != NO_ERROR) {
        Logf("(%s) error failed to open FITS file %s\n", function, info.fits_name.c_str());
        return;
      }

      // Lock the mutex and set the semaphore for file writing
      //
      const std::lock_guard<std::mutex> lock(self->fits_mutex);
      self->writing_file = true;

      // write the primary image into the FITS file
      //
      try {
        long fpixel(1);        // start with the first pixel always
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
      self->close_file();
      Logf("(%s) closed FITS file %s\n", function, info.fits_name.c_str());
      self->writing_file = false;
    }
    /**************** FITS_file::write_image_thread ***************************/


    /**************** FITS_file::make_camera_header ***************************/
    /**
     * @fn     make_camera_header
     * @brief  this writes header info from the camera_info class
     * @param  Information& info, reference to the camera_info class
     * @return 
     *
     * Uses CCFits
     */
    void make_camera_header(Archon::Information &info) {
      const char* function = "FITS_file::make_camera_header";
      try {
        std::string build(BUILD_DATE); build.append(" "); build.append(BUILD_TIME);
        this->pFits->pHDU().addKey("SERV_VER", build, "server build date");
      }
      catch (CCfits::FitsError & err) {
        Logf("(%s) error creating FITS header: %s\n", function, err.message().c_str());
      }
    }
    /**************** FITS_file::make_camera_header ***************************/


    /**************** FITS_file::add_user_key *********************************/
    /**
     * @fn     add_user_key
     * @brief  this writes user-added keywords to the FITS file header
     * @param  std::string keyword
     * @param  std::string type
     * @param  std::string value
     * @param  std::string comment
     * @return nothing
     *
     * Uses CCFits
     */
    void add_user_key(std::string keyword, std::string type, std::string value, std::string comment) {
      const char* function = "FITS_file::add_user_key";
      if (type.compare("INT") == 0) {
        this->pFits->pHDU().addKey(keyword, atoi(value.c_str()), comment);
      }
      else if (type.compare("FLOAT") == 0) {
        this->pFits->pHDU().addKey(keyword, atof(value.c_str()), comment);
      }
      else if (type.compare("STRING") == 0) {
        this->pFits->pHDU().addKey(keyword, value, comment);
      }
      else {
        Logf("(%s) error unknown type: %s for user keyword: %s = %s / %s", function, 
             type.c_str(), keyword.c_str(), value.c_str(), comment.c_str());
      }
    }
    /**************** FITS_file::add_user_key *********************************/

};
#endif
