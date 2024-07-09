#ifndef CAMERA_FITS_H
#define CAMERA_FITS_H
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
#include <filesystem>
#include <fstream>         /// for ofstream
#include <thread>
#include <atomic>
#include <string>
#include "common.h"
#include "build_date.h"
#include "utilities.h"
#include "logentry.h"

const int FITS_WRITE_WAIT = 5000;                   /// approx time (in msec) to wait for a frame to be written

class FITS_file {
  private:
    std::atomic<bool> writing_file;                 /// semaphore indicates file is being written
    std::atomic<bool> error;                        /// indicates an error occured in a file writing thread
    std::atomic<bool> file_open;                    /// semaphore indicates file is open
    std::atomic<int> threadcount;                   /// keep track of number of write_image_thread threads
    std::atomic<int> framen;                        /// internal frame counter for multi-extensions

    std::mutex fits_mutex;                          /// used to block writing_file semaphore in multiple threads
    std::unique_ptr<CCfits::FITS> pFits;            /// pointer to FITS data container
    CCfits::ExtHDU* imageExt;                       /// image extension header unit
    std::string fits_name;

    inline static const std::string in_process = ".writing";  /// add this extension while fits writing is in process, remove on close

  public:
    bool iserror() { return this->error; };         /// allows outsiders access to errors that occurred in a fits writing thread
    bool isopen()  { return this->file_open; };     /// allows outsiders access file open status

    // Default constructor, only initializes values
    //
    FITS_file() : writing_file(false), error(false), file_open(false), threadcount(0), framen(0) { }


    /**************** FITS_file::open_file ************************************/
    /**
     * @fn         open_file
     * @brief      opens a FITS file
     * @param[in]  Information& info, reference to camera_info class
     * @return     ERROR or NO_ERROR
     *
     * This uses CCFits to create a FITS container, opens the file and writes
     * primary header data to it.
     *
     */
    long open_file( bool writekeys, Camera::Information & info ) {
      std::string function = "FITS_file::open_file";
      std::stringstream message;

      const std::lock_guard<std::mutex> lock(this->fits_mutex);

//    int num_axis = ( info.cubedepth > 1 ? 3 : 2 );  // local variable for number of axes
      int num_axis = ( info.fitscubed > 1 ? 3 : 2 );  // local variable for number of axes
      long* axes;                                     // local variable of image axes size

      // Save the requested filename in the FITS_file class.
      // The requested name in the info class is the final name, but the local name in
      // the FITS_file class will add a ".writing" extension when it is opened, which
      // will be used for writing. This will get removed after the file is closed.
      //
      this->fits_name = info.fits_name + this->in_process;

      // This is probably a programming error, if file_open is true here
      //
      if ( this->file_open.load( std::memory_order_seq_cst ) ) {
        message.str(""); message << "ERROR: FITS file \"" << this->fits_name << "\" already open";
        logwrite(function, message.str());
        return (ERROR);
      }

      // Check that we can write the file, because CCFits will crash if it cannot
      //
      std::ofstream checkfile ( this->fits_name.c_str() );
      if ( checkfile.is_open() ) {
        checkfile.close();
        std::remove( this->fits_name.c_str() );
      }
      else {
        message.str(""); message << "ERROR unable to create file \"" << this->fits_name << "\"";
        logwrite(function, message.str());
        return(ERROR);
      }

      axes = new long[num_axis];

      if (info.ismex) {      // special num_axis, axes for multi-extensions, no data associated with primary header
        for ( int i=0; i < num_axis; i++ ) axes[i]=0;
        num_axis = 0;
      }
      else {                 // or regular for flat fits files
        for ( int i=0; i < num_axis; i++ ) axes[i]=info.axes[i];
      }

#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] cubedepth=" << info.cubedepth << " fitscubed=" << info.fitscubed 
                               << " num_axis=" << num_axis << " axes=";
      for ( int na=0; na < num_axis; na++ ) message << axes[na] << " ";
      logwrite( function, message.str() );
#endif

      if (!info.type_set) { // This is a programming error, means datatype is uninitialized.
        logwrite(function, "ERROR: FITS datatype is uninitialized. Call set_axes()");
      }

      try {
        // Create a new FITS object, specifying the data type and axes for the primary image.
        // Simultaneously create the corresponding file.
        //
        this->pFits.reset( new CCfits::FITS(this->fits_name, info.datatype, num_axis, axes) );
        this->file_open = true;    // file is open now
//      this->make_camera_header(info); /// TODO this call might be obsolete

        // Iterate through the system-defined FITS keyword databases and add them to the primary header.
        //
        Common::FitsKeys::fits_key_t::iterator keyit;
        for (keyit  = info.systemkeys.keydb.begin();
             keyit != info.systemkeys.keydb.end();
             keyit++) {
          this->add_key( true, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
        }

        // If specified, iterate through the user-defined FITS keyword databases and add them to the primary header.
        //
        if ( writekeys ) {
          logwrite( function, "writing user-defined keys before exposure" );
          for (keyit  = info.userkeys.keydb.begin();
               keyit != info.userkeys.keydb.end();
               keyit++) {
            this->add_key( true, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
          }
        }
      }
      catch (CCfits::FITS::CantCreate){
        message.str(""); message << "ERROR: unable to open FITS file \"" << this->fits_name << "\"";
        logwrite(function, message.str());
        delete [] axes;
        return(ERROR);
      }
      catch (...) {
        message.str(""); message << "unknown error opening FITS file \"" << this->fits_name << "\"";
        logwrite(function, message.str());
        delete [] axes;
        return(ERROR);
      }

      message.str(""); message << "opened file \"" << this->fits_name << "\" for FITS write";
      logwrite(function, message.str());

      // must reset variables as when container was constructed
      // each time a new file is opened
      //
      this->threadcount = 0;
      this->framen = 0;
      this->writing_file = false;
      this->error = false;

      delete [] axes;
      return (0);
    }
    /**************** FITS_file::open_file ************************************/


    /**************** FITS_file::close_file ***********************************/
    /**
     * @fn         close_file
     * @brief      closes fits file
     * @param[in]  none
     * @return     none
     *
     * Before closing the file, DATE and CHECKSUM keywords are added.
     * Nothing called returns anything so this doesn't return anything.
     *
     */
    void close_file( bool writekeys, Camera::Information & info ) {
      std::string function = "FITS_file::close_file";
      std::stringstream message;

      // Nothing to do if not open
      //
      if ( ! this->file_open ) {
#ifdef LOGLEVEL_DEBUG
        logwrite(function, "[DEBUG] no open FITS file to close");
#endif
        return;
      }

/**
      // Iterate through the system-defined FITS keyword databases and add them to the primary header.
      //
      Common::FitsKeys::fits_key_t::iterator keyit;
      for (keyit  = info.systemkeys.keydb.begin();
           keyit != info.systemkeys.keydb.end();
           keyit++) {
        this->add_key( true, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
      }
**/

      // Write the user keys on close, if specified
      //
      if ( writekeys ) {
        logwrite( function, "writing user-defined keys after exposure" );
        Common::FitsKeys::fits_key_t::iterator keyit;
        try {
          for (keyit  = info.userkeys.keydb.begin();
               keyit != info.userkeys.keydb.end();
               keyit++) {
            this->add_key( true, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
          }
        }
        catch ( const std::exception &e ) {
          message.str(""); message << "ERROR writing user keys on close: " << e.what();
          logwrite( function, message.str() );
        }
      }

      try {
        // Get the date- and time-only out of info.start_time into separate keywords
        //
        std::vector<std::string> datevec;
        std::string dateobs, timeobs;
        Tokenize( info.start_time, datevec, "T" );      // format is "YYYY-MM-DDTHH:MM:SS.s"
        if ( datevec.size() == 2 ) {
          dateobs = datevec.at(0);                      // date-only is the first of two tokens
          timeobs = datevec.at(1);                      // time-only is the second of two tokens
        }

        // Add a few final system keywords
        //
        this->pFits->pHDU().addKey("DATE-BEG", info.start_time, "exposure start time" );
        this->pFits->pHDU().addKey("DATE-END", info.stop_time, "exposure stop time" );
        this->pFits->pHDU().addKey("DATE", get_timestamp(), "FITS file write time");
        this->pFits->pHDU().addKey("COMPSTAT", ( info.exposure_aborted ? "aborted" : "completed" ) , "exposure completion status");
        this->pFits->pHDU().addKey("DATE-CMD", info.cmd_start_time, "time of expose command" );
        this->pFits->pHDU().addKey("DATE-OBS", dateobs, "exposure start date" );
        this->pFits->pHDU().addKey("TIME-OBS", timeobs, "exposure start time" );

        // Write the checksum
        //
        this->pFits->pHDU().writeChecksum();
      }
      catch (CCfits::FitsError& error){
        message.str(""); message << "ERROR writing checksum and closing file: " << error.message();
        logwrite(function, message.str());
        this->file_open.store( false, std::memory_order_seq_cst );   // must set this false on exception
      }
      catch ( std::out_of_range &e ) {
        message.str(""); message << "ERROR parsing cmd_start_time: " << e.what();
        logwrite(function, message.str());
        this->file_open.store( false, std::memory_order_seq_cst );   // must set this false on exception
      }

      // Deallocate the CCfits object and close the FITS file
      //
      this->pFits->destroy();

      // Let the world know that the file is closed
      //
      this->file_open.store( false, std::memory_order_seq_cst );
      message.str(""); message << this->fits_name << " closed";
      logwrite(function, message.str());

      // Rename the file to remove the in_process extension
      //
      std::string finished_file { this->fits_name, 0, this->fits_name.rfind( this->in_process ) };  // remove extension

      std::filesystem::path in_process_path( this->fits_name );
      std::filesystem::path finished_path( finished_file );

      try {
        std::filesystem::rename( in_process_path, finished_path );
        message.str(""); message << "renamed " << this->fits_name << " to " << finished_file;
        logwrite(function, message.str());
      }
      catch ( const std::filesystem::filesystem_error &e ) {
        message.str(""); message << "ERROR renaming " << this->fits_name << " to " << finished_file << ": " << e.what();
        logwrite(function, message.str());
      }

      this->fits_name="";
    }
    /**************** FITS_file::close_file ***********************************/


    /**************** FITS_file::write_image **********************************/
    /**
     * @fn         write_image
     * @brief      spawn threads to write image data to FITS file on disk
     * @param[in]  T* data, pointer to the data using template type T
     * @param[in]  Information& into, reference to the fits_info class
     * @return     ERROR or NO_ERROR
     *
     * This function spawns a thread to write the image data to disk
     *
     */
    template <class T>
    long write_image(T* data, Camera::Information& info) {
      std::string function = "FITS_file::write_image";
      std::stringstream message;

      if ( info.section_size==0 ) {
        logwrite( function, "ERROR: section size is zero!" );
        return(ERROR);
      }

      // The file must have been opened first
      //
      if ( !this->file_open.load( std::memory_order_seq_cst ) ) {
        message.str(""); message << "ERROR: FITS file \"" << this->fits_name << "\" not open";
        logwrite(function, message.str());
        return (ERROR);
      }

      // copy the data into an array of the appropriate type  //TODO use multiple threads for this??
      //
      std::valarray<T> array( info.section_size );
      for ( long i = 0; i < info.section_size; i++ ) {
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

#ifdef LOGLEVEL_DEBUG
      long num_axis = ( info.cubedepth > 1 ? 3 : 2 );
      long axes[num_axis];
      for ( int i=0; i < num_axis; i++ ) axes[i] = info.axes[i];
      message.str("");
      message << "[DEBUG] threadcount=" << this->threadcount << " ismex=" << info.ismex << " section_size=" << info.section_size 
              << " cubedepth=" << info.cubedepth
              << " axes=";
      for ( auto aa : axes ) message << aa << " ";
      message << ". spawning image writing thread for frame " << this->framen << " of " << this->fits_name;
      logwrite(function, message.str());
#endif
      if (!array.size()) {
        logwrite( function, "ERROR: empty array" );
        return(ERROR);
      }
      std::thread([&]() {                                    // create the detached thread here
        if (info.ismex) {
          this->write_mex_thread(array, info, this);
        }
        else {
          this->write_image_thread(array, info, this);
        }
        this->threadcount--;                                 // decrement threadcount
      }).detach();
#ifdef LOGLEVEL_DEBUG
      message.str("");
      message << "[DEBUG] spawned image writing thread for frame " << this->framen << " of " << this->fits_name;
      logwrite(function, message.str());
#endif

      // wait for all threads to complete
      //
      int last_threadcount = this->threadcount.load( std::memory_order_seq_cst );
      int wait = FITS_WRITE_WAIT;
      while (this->threadcount > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (this->threadcount.load( std::memory_order_seq_cst ) >= last_threadcount) {  // threads are not completing
          wait--;                                                                       // start decrementing wait timer
        }
        else {                                                                          // a thread was completed so things are still working
          last_threadcount = this->threadcount.load( std::memory_order_seq_cst );       // reset last threadcount
          wait = FITS_WRITE_WAIT;                                                       // reset wait timer
        }
        if (wait < 0) {
          message.str(""); message << "ERROR: timeout waiting for threads."
                                   << " threadcount=" << threadcount 
                                   << " extension=" << info.extension 
                                   << " framen=" << this->framen
                                   << " file=" << this->fits_name;
          logwrite(function, message.str());
          this->writing_file.store( false, std::memory_order_seq_cst );
          return (ERROR);
        }
      }

      if ( this->error.load( std::memory_order_seq_cst ) ) {
        message.str("");
        message << "an error occured in one of the FITS writing threads for " << this->fits_name;
        logwrite(function, message.str());
      }
#ifdef LOGLEVEL_DEBUG
      else {
        message.str("");
        message << "[DEBUG] " << this->fits_name << " complete";
        logwrite(function, message.str());
      }
#endif

      return ( this->error.load( std::memory_order_seq_cst ) ? ERROR : NO_ERROR );
    }
    /**************** FITS_file::write_image **********************************/


    /**************** FITS_file::write_image_thread ***************************/
    /**
     * @fn         write_image_thread
     * @brief      This is where the data are actually written for flat fits files
     * @param[in]  T &data, reference to the data
     * @param[in]  Camera::Information &info, reference to the info structure
     * @param[in]  FITS_file *self, pointer to this-> object
     * @return     nothing
     *
     * This is the worker thread, to write the data using CCFits,
     * and must be spawned by write_image.
     *
     */
    template <class T>
    void write_image_thread(std::valarray<T> &data, Camera::Information &info, FITS_file *self) {
      std::string function = "FITS_file::write_image_thread";
      std::stringstream message;

      // This makes the thread wait while another thread is writing images. This
      // function is really for single image writing, it's here just in case.
      //
      int wait = FITS_WRITE_WAIT;
      while ( self->writing_file.load( std::memory_order_seq_cst ) ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (--wait < 0) {
          message.str(""); message << "ERROR: timeout waiting for last frame to complete. "
                                   << "unable to write " << this->fits_name;
          logwrite(function, message.str());
          self->writing_file.store( false, std::memory_order_seq_cst );
          self->error = true;    // tells the calling function that I had an error
          return;
        }
      }

      // Lock the mutex and set the semaphore for file writing
      //
      const std::lock_guard<std::mutex> lock(self->fits_mutex);
      self->writing_file.store( true, std::memory_order_seq_cst );

      // Set the FITS system to verbose mode so it writes error messages
      //
      CCfits::FITS::setVerboseMode(true);

      // write the primary image into the FITS file
      //
      try {
        if (info.datatype == SHORT_IMG) {
          this->pFits->pHDU().addKey("BZERO", 32768, "offset for signed short int" );
          this->pFits->pHDU().addKey("BSCALE", 1, "scaling factor");
        }
        else {
          this->pFits->pHDU().addKey("BZERO", 0.0, "offset");
          this->pFits->pHDU().addKey("BSCALE", 1, "scaling factor");
        }

        long fpixel(1);        // start with the first pixel always
        self->pFits->pHDU().write( fpixel, info.section_size, data );
        self->pFits->flush();  // make sure the image is written to disk
      }
      catch (CCfits::FitsError& error){
        message.str(""); message << "FITS file error thrown: " << error.message();
        logwrite(function, message.str());
        self->writing_file.store( false, std::memory_order_seq_cst );
        self->error = true;    // tells the calling function that I had an error
        return;
      }

      self->writing_file.store( false, std::memory_order_seq_cst );
    }
    /**************** FITS_file::write_image_thread ***************************/


    /**************** FITS_file::write_mex_thread *****************************/
    /**
     * @fn         write_mex_thread
     * @brief      This is where the data are actually written for multi-extensions
     * @param[in]  T &data, reference to the data
     * @param[in]  Camera::Information &info, reference to the info structure
     * @param[in]  FITS_file *self, pointer to this-> object
     * @return     nothing
     *
     * This is the worker thread, to write the data using CCFits,
     * and must be spawned by write_image.
     *
     */
    template <class T>
    void write_mex_thread(std::valarray<T> &data, Camera::Information &info, FITS_file *self) {
      std::string function = "FITS_file::write_mex_thread";
      std::stringstream message;

#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] " << this->fits_name << ": info.extension=" << info.extension << " this->framen=" << this->framen << " axes=";
      for ( auto aa : info.axes ) message << aa << " ";
      logwrite( function, message.str() );
#endif

      if ( self==nullptr ) {
        logwrite( function, "ERROR: bad self pointer" );
        return;
      }
      if ( !data.size() ) {
        logwrite( function, "ERROR: bad data" );
        return;
      }

#ifdef LOGLEVEL_DEBUG
      if (std::is_same<T, float>::value) {
        logwrite( function, "[DEBUG] input type is float" );
      }
      else if (std::is_same<T, double>::value) {
        logwrite( function, "[DEBUG] input type is double" );
      }
      else if (std::is_same<T, short>::value) {
        logwrite( function, "[DEBUG] input type is short" );
      }
      else if (std::is_same<T, unsigned short>::value) {
        logwrite( function, "[DEBUG] input type is unsigned short" );
      }
      else {
        message.str(""); message << "[DEBUG] input type is " << typeid(T).name();
        logwrite( function, message.str() );
      }
#endif

      // This makes the thread wait while another thread is writing images. This
      // thread will only start writing once the extension number matches the
      // number of frames already written.
      //
      int last_threadcount = this->threadcount.load( std::memory_order_seq_cst );
      int wait = FITS_WRITE_WAIT;
      while ( info.extension != this->framen.load( std::memory_order_seq_cst ) ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (this->threadcount.load( std::memory_order_seq_cst ) >= last_threadcount) {  // threads are not completing
          wait--;                                                                       // start decrementing wait timer
        }
        else {                                                                          // a thread was completed so things are still working
          last_threadcount = this->threadcount.load( std::memory_order_seq_cst );       // reset last threadcount
          wait = FITS_WRITE_WAIT;                                                       // reset wait timer
        }
        if (wait < 0) {
          message.str(""); message << "ERROR: timeout waiting for frame write."
                                   << " threadcount=" << threadcount 
                                   << " extension=" << info.extension 
                                   << " framen=" << this->framen;
          logwrite(function, message.str());
          self->writing_file.store( false, std::memory_order_seq_cst );
          self->error = true;    // tells the calling function that I had an error
          return;
        }
      }

      // Set the FITS system to verbose mode so it writes error messages
      //
      CCfits::FITS::setVerboseMode(true);

      // Lock the mutex and set the semaphore for file writing
      //
      const std::lock_guard<std::mutex> lock(self->fits_mutex);
      self->writing_file.store( true, std::memory_order_seq_cst );

      // write the primary image into the FITS file
      //
      try {
        long fpixel(1);                     // start with the first pixel always

//      long num_axis = ( info.cubedepth > 1 ? 3 : 2 );
        long num_axis = ( info.fitscubed > 1 ? 3 : 2 );

        std::vector<long> axes(num_axis);   // addImage() wants a vector, which has the size of the number of axes

        for ( int i=0; i < num_axis; i++ ) axes[i]=info.axes[i];

        // create the extension name
        // This shows up as keyword EXTNAME and in DS9's "display header"
        //
        std::string extname = std::to_string( info.extension+1 );

        message.str("");     message << "adding " << axes[0] << " x " << axes[1];
        if ( num_axis==3 ) { message << " x " << axes[2]; }
                             message << " frame to extension " << extname << " in file " << this->fits_name;
        logwrite(function, message.str());

        // Add the extension here
        //
        this->imageExt = self->pFits->addImage(extname, info.datatype, axes);

        // Add extension-only keys now
        //
        if (info.datatype == SHORT_IMG) {
          this->imageExt->addKey("BZERO", 32768, "offset for signed short int");
          this->imageExt->addKey("BSCALE", 1, "scaling factor");
        }
        else {
          this->imageExt->addKey("BZERO", 0.0, "offset");
          this->imageExt->addKey("BSCALE", 1, "scaling factor");
        }

        Common::FitsKeys::fits_key_t::iterator keyit;
        for ( keyit  = info.extkeys.keydb.begin();
              keyit != info.extkeys.keydb.end();
              keyit++ ) {
          this->add_key( false, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
        }

        // Add AMPSEC keys
        //
/*
        if ( info.amp_section.size() > 0 ) {
          try {
            int x1 = info.amp_section.at( info.extension ).at( 0 );
            int x2 = info.amp_section.at( info.extension ).at( 1 );
            int y1 = info.amp_section.at( info.extension ).at( 2 );
            int y2 = info.amp_section.at( info.extension ).at( 3 );

            message.str(""); message << "[" << x1 << ":" << x2 << "," << y1 << ":" << y2 << "]";
            this->imageExt->addKey( "AMPSEC", message.str(), "amplifier section" );
          }
          catch ( std::out_of_range & ) {
            logwrite( function, "ERROR: no amplifier section referenced for this extension" );
          }
        }
        else {
          logwrite( function, "no AMPSEC key: missing amplifier section information" );
        }
*/

        // Write and flush to make sure image is written to disk
        //
        if ( this->imageExt) this->imageExt->write( fpixel, info.section_size, data );
        else {
          logwrite( function, "ERROR: imageExt is NULL!" );
          return;
        }
        self->pFits->flush();
      }
      catch (CCfits::FitsError& error){
        message.str(""); message << "FITS file error thrown: " << error.message();
        logwrite(function, message.str());
        self->writing_file.store( false, std::memory_order_seq_cst );
        self->error = true;    // tells the calling function that I had an error
        return;
      }

      // increment number of frames written
      //
      this->framen++;
      self->writing_file.store( false, std::memory_order_seq_cst );
    }
    /**************** FITS_file::write_mex_thread *****************************/


    /**************** FITS_file::make_camera_header ***************************/
    /**
     * @fn         make_camera_header
     * @brief      this writes header info from the camera_info class
     * @param[in]  Information& info, reference to the camera_info class
     * @return     none
     *
     * Uses CCFits
     *
     * TODO is this function obsolete?
     *
     */
    void make_camera_header(Camera::Information &info) {
      std::string function = "FITS_file::make_camera_header";
      std::stringstream message;
      try {
      }
      catch (CCfits::FitsError & err) {
        message.str(""); message << "error creating FITS header: " << err.message();
        logwrite(function, message.str());
      }
    }
    /**************** FITS_file::make_camera_header ***************************/


    /***** FITS_file::add_key *************************************************/
    /**
     * @brief      wrapper to write keywords to the FITS file header
     * @param[in]  primary  boolean is true for primary, false for extension
     * @param[in]  keyword
     * @param[in]  type
     * @param[in]  value
     * @param[in]  comment
     *
     * Uses CCFits addKey template function, this wrapper ensures the correct type is passed.
     * 
     */
    void add_key( bool primary, std::string keyword, std::string type, std::string value, std::string comment ) {
      std::string function = "FITS_file::add_key";
      std::stringstream message;

#ifdef LOGLEVEL_DEBUG
      message << "[DEBUG] keyword=" << keyword << " type=" << type << " value=" << value << " comment=" << comment;
      logwrite( function, message.str() );
#endif

      // The file must have been opened first
      //
      if ( !this->file_open.load( std::memory_order_seq_cst ) ) {
        logwrite(function, "ERROR: no fits file open!");
        return;
      }

      try {
        if (type.compare("BOOL") == 0) {
          bool boolvalue = ( value == "T" ? true : false );
          ( primary ? this->pFits->pHDU().addKey( keyword, boolvalue, comment )
                    : this->imageExt->addKey( keyword, boolvalue, comment ) );
        }
        else if (type.compare("INT") == 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, std::stoi(value), comment)
                    : this->imageExt->addKey( keyword, std::stoi(value), comment ) );
        }
        else if (type.compare("LONG") == 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, std::stol(value), comment)
                    : this->imageExt->addKey( keyword, std::stol(value), comment ) );
        }
        else if (type.compare("FLOAT") == 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, std::stof(value), comment)
                    : this->imageExt->addKey( keyword, std::stof(value), comment ) );
        }
        else if (type.compare("DOUBLE") == 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, std::stod(value), comment)
                    : this->imageExt->addKey( keyword, std::stod(value), comment ) );
        }
        else if (type.compare("STRING") == 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, value, comment)
                    : this->imageExt->addKey( keyword, value, comment ) );
        }
        else {
          message.str(""); message << "ERROR unknown type: " << type << " for user keyword: " << keyword << "=" << value
                                   << ": expected {INT,LONG,FLOAT,DOUBLE,STRING,BOOL}";
          logwrite(function, message.str());
        }
      }
      // There could be an error converting a value to INT or FLOAT with stoi or stof,
      // in which case save the keyword as a STRING.
      //
      catch ( std::invalid_argument & ) {
        message.str(""); message << "ERROR: unable to convert value " << value;
        logwrite( function, message.str() );
        if (type.compare("STRING") != 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, value, comment)
                    : this->imageExt->addKey( keyword, value, comment ) );
        }
      }
      catch ( std::out_of_range & ) {
        message.str(""); message << "ERROR: value " << value << " out of range";
        logwrite( function, message.str() );
        if (type.compare("STRING") != 0) {
          ( primary ? this->pFits->pHDU().addKey(keyword, value, comment)
                    : this->imageExt->addKey( keyword, value, comment ) );
        }
      }
      catch (CCfits::FitsError & err) {
        message.str(""); message << "ERROR adding key " << keyword << "=" << value << " / " << comment << " (" << type << ")"
                                 << " to " << ( primary ? "primary" : "extension" ) << " :"
                                 << err.message();
        logwrite(function, message.str());
      }
    }
    /***** FITS_file::add_key *************************************************/

};
#endif
