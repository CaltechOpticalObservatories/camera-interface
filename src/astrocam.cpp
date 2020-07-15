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


  /** AstroCam::AstroCam::native **********************************************/
  /**
   * @fn     native
   * @brief  send a 3-letter command to the Leach controller
   * @param  cmdstr, string containing command and arguments
   * @return 0 on success, 1 on error
   *
   */
  long AstroCam::native(std::string cmdstr) {
    std::string function = "AstroCam::AstroCam::native";
    std::stringstream message;
    std::vector<std::string> tokens;

    int arg[4]={-1,-1,-1,-1};
    int cmd=0, c0, c1, c2;

    Tokenize(cmdstr, tokens, " ");

    int nargs = tokens.size() - 1;  // one token is for the command, this is number of args

    // max 4 arguments
    //
    if (nargs > 4) {
      message.str(""); message << "error: too many arguments: " << nargs << " (max 4)";
      logwrite(function, message.str());
      return(1);
    }

    // convert each arg into a number
    //
    for (int i=0,j=1; i<nargs; i++,j++) {
      arg[i] = parse_val(tokens[j]);
    }

    // first token is command and require a 3-letter command
    //
    if (tokens[0].length() != 3) {
      message.str(""); message << "error: unrecognized command: " << tokens[0];
      logwrite(function, message.str());
      return(1);
    }

    // change the 3-letter (ASCII) command into hex byte representation
    //
    c0  = (int)tokens[0].at(0); c0 = c0 << 16;
    c1  = (int)tokens[0].at(1); c1 = c1 <<  8;
    c2  = (int)tokens[0].at(2);
    cmd = c0 | c1 | c2;

    message.str(""); message << "sending command: " 
                             << std::setfill('0') << std::setw(2) << std::uppercase << std::hex
                             << "0x" << cmd
                             << " 0x" << arg[0]
                             << " 0x" << arg[1]
                             << " 0x" << arg[2]
                             << " 0x" << arg[3];
    logwrite(function, message.str());

    // send the command here to the interface
    //
    return this->arc_native(cmd, arg[0], arg[1], arg[2], arg[3]);

  }
  /** AstroCam::AstroCam::native **********************************************/


  /** AstroCam::AstroCam::get_parameter ***************************************/
  /**
   * @fn     get_parameter
   * @brief  
   * @param  
   * @return 
   *
   */
  long AstroCam::get_parameter(std::string parameter, std::string &retstring) {
    std::string function = "AstroCam::AstroCam::get_parameter";
    logwrite(function, "error: invalid command for this controller");
    return(ERROR);
  }
  /** AstroCam::AstroCam::get_parameter ***************************************/


  /** AstroCam::AstroCam::set_parameter ***************************************/
  /**
   * @fn     set_parameter
   * @brief  
   * @param  
   * @return 
   *
   */
  long AstroCam::set_parameter(std::string parameter) {
    std::string function = "AstroCam::AstroCam::set_parameter";
    logwrite(function, "error: invalid command for this controller");
    return(ERROR);
  }
  /** AstroCam::AstroCam::set_parameter ***************************************/


  /** AstroCam::AstroCam::nframes *********************************************/
  /**
   * @fn     nframes
   * @brief  
   * @param  
   * @return 
   *
   */
  long AstroCam::access_nframes(std::string valstring) {
    std::string function = "AstroCam::AstroCam::nframes";
    std::stringstream message;
    std::vector<std::string> tokens;

    Tokenize(valstring, tokens, " ");

    if (tokens.size() != 2) {
      message.str(""); message << "error: nframes expected 1 value but got " << tokens.size()-1;
      logwrite(function, message.str());
      return(ERROR);
    }
    int rows = this->get_rows();
    int cols = this->get_cols();

    this->nfpseq  = parse_val(tokens[1]);            // requested nframes is nframes/sequence
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

//TODO
/**
    fitskeystr.str(""); fitskeystr << "NFRAMES=" << this->nframes << "//number of frames";
    this->fitskey.set_fitskey(fitskeystr.str()); // TODO
**/

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

    return 0;
  }
  /** AstroCam::AstroCam::nframes *********************************************/


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
