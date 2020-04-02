/**
 * @file    fits.h
 * @brief   fits interface functions header file
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include <CCfits/CCfits>
#include <cmath>
#include <memory>

template <class T>
class FITS_file {
  private:
    std::unique_ptr<CCfits::FITS> pFits;            //!< pointer to FITS data container
    std::string fits_name;                          //!< name for FITS file
    int num_frames;

  public:
    FITS_file() {
      this->num_frames = 0;
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

      try {
        // Allocate the FITS file container, which holds the information used by CCfits to write a file
        //
        this->pFits.reset( new CCfits::FITS(info.image_name, info.bitpix, info.naxis, info.naxes) );
      }
      catch (CCfits::FITS::CantCreate){
        Logf("(%s) error: unable to open FITS file %s\n", function, info.image_name.c_str());
        return(ERROR);
      }   
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

      // Add a header keyword for the time the file was written (right now!)
      //
      this->pFits->pHDU().addKey("DATE", get_fits_time(), "FITS file write date");

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

    std::string get_fits_time() {
      std::stringstream current_time;  // String to contain the time
      time_t t;                        // Container for system time
      struct timespec data;            // Time of day container
      struct tm gmt;                   // GMT time container

      // Get the system time, return a bad timestamp on error
      //
      if (clock_gettime(CLOCK_REALTIME, &data) != 0) return("9999-99-99T99:99:99.999");

      // Convert the time of day to GMT
      //
      t = data.tv_sec;
      if (gmtime_r(&t, &gmt) == NULL) return("9999-99-99T99:99:99.999");
 
      current_time.setf(std::ios_base::right);
      current_time << std::setfill('0') << std::setprecision(0)
      << std::setw(4) << gmt.tm_year + 1900   << "-"
      << std::setw(2) << gmt.tm_mon + 1 << "-"
      << std::setw(2) << gmt.tm_mday    << "T"
      << std::setw(2) << gmt.tm_hour  << ":"
      << std::setw(2) << gmt.tm_min << ":"
      << std::setw(2) << gmt.tm_sec
      << "." << std::setw(3) << data.tv_nsec/1000000;

      return(current_time.str());
    }

};
