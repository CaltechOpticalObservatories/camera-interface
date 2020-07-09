#include "astrocam.h"

namespace AstroCam {

  AstroCam::AstroCam() {
    this->config.filename = CONFIG_FILE;  //!< default config file set in CMakeLists.txt
  }

  AstroCam::~AstroCam() {
  }

  /** AstroCam::AstroCam::configure_controller ********************************/
  /**
   * @fn     configure_controller
   * @brief  callback function for frame received
   * @param  
   * @return none
   *
   */
  long AstroCam::configure_controller() {
    std::string function = "AstroCam::AstroCam::configure_controller";

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      if (config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->common.imdir( config.arg[entry] );
      }

      if (config.param[entry].compare(0, 6, "IMNAME")==0) {
        this->common.imname( config.arg[entry] );
      }

    }
    logwrite(function, "successfully applied configuration");
    return NO_ERROR;
  }
  /** AstroCam::AstroCam::configure_controller ********************************/


  /** AstroCam::AstroCam::set_something ***************************************/
  /**
   * @fn     set_something
   * @brief  
   * @param  
   * @return 
   *
   */
  long AstroCam::set_something(std::string something) {
    std::string function = "AstroCam::AstroCam::set_something";
    std::stringstream message;
    size_t pos = something.find_first_of(" ");             // position of the first space : to delimit the param name

    std::string paramname = something.substr(0, pos);      // the param name : first space-delimited argument
    std::string arg = something.erase(0, pos + 1);         // the argument(s) : everything else

    std::stringstream fitskeystr;

    int  value = parse_val(arg);                           // parse arg as an integer : many things use this

    // set nframes
    //
    if (paramname == "nframes") {
      int rows = this->get_rows();
      int cols = this->get_cols();

      this->nfpseq  = value;                           // requested nframes is nframes/sequence
      this->nframes = this->nfpseq * this->nsequences; // number of frames is (frames/sequence) x (sequences)
      std::stringstream snf;
      snf << "SNF " << this->nframes;                  // SNF sets total number of frames (Y:<N_FRAMES) on timing board

      message.str(""); message << "sending " << snf;
      logwrite(function, message.str());

      if ( this->native(snf.str()) != 0x444F4E ) return -1;

      std::stringstream fps;
      fps << "FPS " << nfpseq;            // FPS sets number of frames per sequence (Y:<N_SEQUENCES) on timing board

      message.str(""); message << "sending " << fps;
      logwrite(function, message.str());

      if ( this->native(fps.str()) != 0x444F4E ) return -1;

      fitskeystr.str(""); fitskeystr << "NFRAMES=" << this->nframes << "//number of frames";
//    this->fitskey.set_fitskey(fitskeystr.str()); // TODO

      int _framesize = rows * cols * sizeof(unsigned short);
      if (_framesize < 1) {
        message.str(""); message << "error: bad framesize: " << _framesize;
        logwrite(function, message.str());
        return (-1);
      }
      unsigned int _nfpb = (unsigned int)( this->get_bufsize() / _framesize );

      if ( (_nfpb < 1) ||
           ( (this->nframes > 1) &&
             (this->get_bufsize() < (int)(2*rows*cols*sizeof(unsigned short))) ) ) {
        message.str(""); message << "insufficient buffer size (" 
                                 << this->get_bufsize()
                                 << " bytes) for "
                                 << this->nframes
                                 << " frame"
                                 << (this->nframes>1 ? "s" : "")
                                 << " of "
                                 << rows << " x " << cols << " pixels";
        logwrite(function, message.str());
        message.str(""); message << "minimum buffer size is "
                                 << 2 * this->nframes * rows * cols
                                 << " bytes";
        logwrite(function, message.str());
        return -1;
      }

      std::stringstream fpb;
      fpb << "FPB " << _nfpb;

      message.str(""); message << "sending " << fpb;
      logwrite(function, message.str());

      if ( this->native(fpb.str()) == 0x444F4E ) return 0; else return -1;

    } // set nframes

    return 0;
  }
  /** AstroCam::AstroCam::set_something ***************************************/


  /** AstroCam::AstroCam::expose **********************************************/
  /**
   * @fn     expose
   * @brief  
   * @param  
   * @return 
   *
   */
  long AstroCam::expose() {
    std::string function = "AstroCam::AstroCam::expose";
    std::stringstream message;
    int rows = this->get_rows();
    int cols = this->get_cols();
    int bufsize = this->get_bufsize();

    // check image size
    //
    if (rows < 1 || cols < 1) {
      message.str(""); message << "error: image size must be non-zero: rows=" << rows << " cols=" << cols;
      logwrite(function, message.str());
      return(1);
    }

    // check buffer size
    // need allocation for at least 2 frames if nframes is greater than 1
    //
    if ( bufsize < (int)( (this->nframes>1?2:1) * rows * cols * sizeof(unsigned short) ) ) {  // TODO type check bufsize: is it big enough?
      message.str(""); message << "error: insufficient buffer size (" << bufsize 
                               << " bytes) for " << this->nframes << " frame" << (this->nframes==1?"":"s")
                               << " of " << rows << " x " << cols << " pixels";
      logwrite(function, message.str());
      message.str(""); message << "minimum buffer size is " << (this->nframes>1?2:1) * rows * cols * sizeof(unsigned short) << " bytes";
      logwrite(function, message.str());
      return(1);
    }
    logwrite(function, "hi");
    this->arc_expose(this->nframes, this->expdelay, rows, cols);
    return 0;
  }
  /** AstroCam::AstroCam::expose **********************************************/


  /** AstroCam::AstroCam::load_firmware ***************************************/
  /**
   * @fn     expose
   * @brief  
   * @param  
   * @return 
   *
   */
  long AstroCam::load_firmware(std::string timlodfile) {
    std::string function = "AstroCam::AstroCam::load_firmware";
    std::stringstream message;
    struct stat st;
    long error = ERROR;

    // check the provided timlodfile string
    //
    if (timlodfile.empty()) {
      logwrite(function, "error: no filename provided");
    }
    else
    if (stat(timlodfile.c_str(), &st) != 0) {
      message.str(""); message << "error: " << timlodfile << " does not exist";
      logwrite(function, message.str());
    }
    else {
      message.str(""); message << "loading " << timlodfile << " into timing board";
      logwrite(function, message.str());
      error = this->arc_load_firmware(timlodfile);
    }

    return( error );
  }
  /** AstroCam::AstroCam::load_firmware ***************************************/
}
