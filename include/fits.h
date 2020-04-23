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
#include <string>
#include "common.h"
#include "build_date.h"
#include "utilities.h"
#include "newlogentry.h"

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
      std::string function = "FITS_file::open_file";
      std::stringstream message;

      const std::lock_guard<std::mutex> lock(this->fits_mutex);

      // Check that we can write the file, because CCFits will crash if it cannot
      //
      std::ofstream checkfile ( info.fits_name.c_str() );
      if ( checkfile.is_open() ) {
        checkfile.close();
        std::remove( info.fits_name.c_str() );
      }
      else {
        message.str(""); message << "error unable to create file " << info.fits_name;
        logwrite(function, message.str());
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
        message.str(""); message << "error: unable to open FITS file " << info.fits_name;
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (...) {
        logwrite(function, "unknown error");
        return(ERROR);
      }

      message.str(""); message << "opened file " << info.fits_name << " for FITS write";
      logwrite(function, message.str());

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

      // Add a header keyword for the time the file was written (right now!)
      //
      this->pFits->pHDU().addKey("DATE", get_time_string(), "FITS file write date");

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
      std::string function = "FITS::write_image";

      if (info.data_cube == true) {
        logwrite(function, "error data cube not supported");
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
        logwrite(function, "waiting for threads to complete");
        while (this->threadcount > 0) usleep(1000);            // wait for all threads to complete
      }

      logwrite(function, "complete");
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
      std::string function = "FITS_file::write_image_thread";
      std::stringstream message;

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
        message.str(""); message << "error failed to open FITS file " << info.fits_name;
        logwrite(function, message.str());
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
        message.str(""); message << "FITS file error thrown: " << error.message();
        logwrite(function, message.str());
        self->writing_file = false;
        return;
      }

      // Close the FITS file
      //
      self->close_file();
      message.str(""); message << "closed FITS file " << info.fits_name;
      logwrite(function, message.str());
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
      std::string function = "FITS_file::make_camera_header";
      std::stringstream message;
      try {
        std::string build(BUILD_DATE); build.append(" "); build.append(BUILD_TIME);
        this->pFits->pHDU().addKey("SERV_VER", build, "server build date");
      }
      catch (CCfits::FitsError & err) {
        message.str(""); message << "error creating FITS header: " << err.message();
        logwrite(function, message.str());
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
      std::string function = "FITS_file::add_user_key";
      std::stringstream message;
      if (type.compare("INT") == 0) {
        this->pFits->pHDU().addKey(keyword, std::stoi(value), comment);
      }
      else if (type.compare("FLOAT") == 0) {
        this->pFits->pHDU().addKey(keyword, std::stof(value), comment);
      }
      else if (type.compare("STRING") == 0) {
        this->pFits->pHDU().addKey(keyword, value, comment);
      }
      else {
        message.str(""); message << "error unknown type: " << type << " for user keyword: " << keyword << "=" << value << " / " << comment;
        logwrite(function, message.str());
      }
    }
    /**************** FITS_file::add_user_key *********************************/

};
#endif
