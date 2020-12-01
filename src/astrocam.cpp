#include "astrocam.h"

namespace AstroCam {

  /** AstroCam::Callback::exposeCallback **************************************/
  /**
   * @fn     exposeCallback
   * @brief  
   * @param  
   * @return none
   *
   */
  void Callback::exposeCallback( float fElapsedTime ) {
    std::string function = "AstroCam::Callback::exposeCallback";
    std::stringstream message;
    message << "elapsed exposure time = " << fElapsedTime;
    std::cerr << message.str();
    for ( uint32_t i=0; i < message.str().length(); i++ ) std::cerr << "\b";
  }
  /** AstroCam::Callback::exposeCallback **************************************/


  /** AstroCam::Callback::readCallback ****************************************/
  /**
   * @fn     readCallback
   * @brief  
   * @param  
   * @return none
   *
   */
  void Callback::readCallback( std::uint32_t uiPixelCount ) {
    std::string function = "AstroCam::Callback::readCallback";
    std::stringstream message;
    message << "pixel count = " << uiPixelCount;
    std::cerr << message.str();
    for ( uint32_t i=0; i < message.str().length(); i++ ) std::cerr << "\b";
  }
  /** AstroCam::Callback::readCallback ****************************************/


  /** AstroCam::Callback::frameCallback ***************************************/
  /**
   * @fn     frameCallback
   * @brief  
   * @param  
   * @return none
   *
   */
  void Callback::frameCallback( std::uint32_t uiFramesPerBuffer, std::uint32_t uiFrameCount, std::uint32_t uiRows, std::uint32_t uiCols, void* pBuffer ) {
    std::string function = "AstroCam::Callback::frameCallback";
    std::stringstream message;
    message << "spawning handle_frame thread: frame=" << uiFrameCount 
            << " buffer=0x" << std::setfill('0') << std::uppercase << std::hex << pBuffer;
    logwrite(function, message.str());
    std::thread( std::ref(AstroCam::Interface::handle_frame), uiFrameCount, pBuffer ).detach();
  }
  /** AstroCam::Callback::frameCallback ***************************************/


  /** AstroCam::Interface::Interface ******************************************/
  /**
   * @fn     Interface
   * @brief  class constructor
   * @param  none
   * @return none
   *
   */
  Interface::Interface() {
    this->modeselected = false;
    this->numdev = 0;
  }
  /** AstroCam::Interface::Interface ******************************************/


  /** AstroCam::Interface::Interface ******************************************/
  /**
   * @fn     ~Interface
   * @brief  class deconstructor
   * @param  none
   * @return none
   *
   */
  Interface::~Interface() {
  }
  /** AstroCam::Interface::Interface ******************************************/


  /** AstroCam::Interface::interface ******************************************/
  /**
   * @fn     interface
   * @brief  returns the interface
   * @param  reference to std::string iface, to return the interface as a string
   * @return NO_ERROR
   *
   */
  long Interface::interface(std::string &iface) {
    std::string function = "AstroCam::Interface::interface";
    iface = "AstroCam";
    logwrite(function, iface);
    return(NO_ERROR);
  }
  /** AstroCam::Interface::interface ******************************************/


  /** AstroCam::Interface::connect_controller *********************************/
  /**
   * @fn     connect_controller
   * @brief  opens a connection to the PCI/e device(s)
   * @param  devices_in, optional string containing space-delimited list of devices
   * @return ERROR or NO_ERROR
   *
   * Input parameter devices_in defaults to empty string which will attempt to
   * connect to all detected devices.
   *
   * If devices_in is specified (and not empty) then it must contain a space-delimited
   * list of device numbers to open. A public vector devlist will hold these device
   * numbers. This vector will be updated here to represent only the devices that
   * are actually connected.
   *
   */
  long Interface::connect_controller(std::string devices_in="") {
    std::string function = "AstroCam::Interface::connect_controller";
    std::stringstream message;

    // Don't allow another open command --
    // Open creates a vector of objects and it's easier to manage by total
    // destruction and construction.
    //
    if (this->controller.size() > 0) {
      logwrite(function, "ERROR: controller connection already open.");
      return(ERROR);
    }

    // Find the installed devices
    //
    arc::gen3::CArcPCI::findDevices();

    this->numdev = arc::gen3::CArcPCI::deviceCount();

    message.str(""); message << "found " << this->numdev << " ARC device" << (this->numdev != 1 ? "s" : "");
    logwrite(function, message.str());

    // Nothing to do if there are no devices detected.
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no devices found");
      return(ERROR);
    }

    // Log all PCI devices found
    //
    const std::string* pdevList;
    pdevList = arc::gen3::CArcPCI::getDeviceStringList();
    for ( uint32_t i=0; i < arc::gen3::CArcPCI::deviceCount(); i++ ) {
      message.str(""); message << "found " << pdevList[ i ];
      logwrite(function, message.str());
    }

    // Look at the requested device(s) to open, which are in the
    // space-delimited string devices_in. The devices to open
    // are stored in a public vector "devlist".
    //

    // If no string is given then build devlist from all detected devices
    //
    if ( devices_in.empty() ) {
      for ( auto n = 0; n < this->numdev; n++ ) this->devlist.push_back( n );
    }

    // Otherwise, tokenize the device list string and build devlist from the tokens
    //
    else {
      std::vector<std::string> tokens;
      Tokenize(devices_in, tokens, " ");
      for ( auto n : tokens ) {                       // For each token in the devices_in string,
        try {
          this->devlist.push_back( std::stoi( n ) );  // convert it to int and push into devlist vector.
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "ERROR: invalid device number: " << n << ": unable to convert to integer";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ERROR: device number " << n << ": out of integer range";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch(...) { logwrite(function, "unknown error getting device number"); return(ERROR); }
      }
    }

    // Create an object for each device in the system and store a pointer to each object in a vector
    //
    for (int dev = 0; dev < this->numdev; dev++) {
      try {
        this->controller.push_back( controller_info() );   // push a new controller_info struct into public vector
        arc::gen3::CArcDevice* pArcDev = NULL;             // create a generic CArcDevice pointer
        pArcDev = new arc::gen3::CArcPCI();                // point this at a new CArcPCI() object  //TODO implement PCIe option
        Callback* pCB = new Callback();                    // create a pointer to a Callback() class object

        this->controller.at(dev).pArcDev = pArcDev;        // set the pointer to this object in the public vector
        this->controller.at(dev).callback = pCB;           // set the pointer to this object in the public vector
        this->controller.at(dev).devnum = dev;             // device number
        this->controller.at(dev).devname = pdevList[dev];  // device name
        this->controller.at(dev).connected = false;        // not yet connected
        this->controller.at(dev).firmwareloaded = false;   // no firmware loaded
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: creating controller object for device number " << dev << ": out of range";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error creating controller object"); return(ERROR); }
    }

    // Open only the devices specified by the devlist vector
    //
    for (auto dev : this->devlist) {
      try {
        // Open the PCI device
        //
        message.str(""); message << "opening device " << dev;
        logwrite(function, message.str());
        this->controller.at(dev).pArcDev->open(dev);

        // Reset the PCI device
        //
        message.str(""); message << "reset PCI board " << dev;
        logwrite(function, message.str());
        this->controller.at(dev).pArcDev->reset();

        // Is Controller Connected?  (tested with a TDL command)
        //
        this->controller.at(dev).connected = this->controller.at(dev).pArcDev->isControllerConnected();
        message.str(""); message << "controller " << dev << " connected = " << (this->controller.at(dev).connected ? "true" : "false");
        logwrite(function, message.str());
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: unable to open  device number " << dev << " because it's not in the list: { ";
        for (auto devcheck : this->devlist) message << devcheck << " ";
        message << "}";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(const std::exception &e) {
        message.str(""); message << "ERROR: " << this->controller.at(dev).devname << ": " << e.what();
        logwrite(function, message.str());
        return (ERROR);
      }
      catch(...) {
        message.str(""); message << "unknown error connecting to " << this->controller.at(dev).devname;
        logwrite(function, message.str());
        return (ERROR);
      }
    }

    // Update the devlist vector with connected controllers only
    //
    for (auto dev : this->devlist) {
      try {
        if ( ! this->controller.at(dev).connected ) {      // If this devnum is not connected then
          this->devlist.at(dev) = -1;                      // mark it for removal.
        }
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: requested device number " << dev << " not in list: { ";
        for (auto devcheck : this->devlist) message << devcheck << " ";
        message << "}";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error updaing device list"); return(ERROR); }
    }

    // Now remove the marked devices (those not connected) from this->devlist
    //
    this->devlist.erase( std::remove (this->devlist.begin(), this->devlist.end(), -1), this->devlist.end() );

    // Log the list of connected devices
    //
    message.str(""); message << "connected devices : { ";
    for (auto devcheck : this->devlist) { message << devcheck << " "; } message << "}";
    logwrite(function, message.str());

    return(0);
  }
  /** AstroCam::Interface::connect_controller *********************************/


  /** AstroCam::Interface::disconnect_controller ******************************/
  /**
   * @fn     disconnect_controller
   * @brief  closes the connection to the PCI/e device(s)
   * @param  none
   * @return NO_ERROR
   *
   * no error handling
   *
   */
  long Interface::disconnect_controller() {
    std::string function = "AstroCam::Interface::disconnect_controller";
    std::stringstream message;

    // close all of the PCI devices
    //
    for (auto dev : this->controller) {
      message.str(""); message << "closing " << dev.devname;
      logwrite(function, message.str());
      dev.pArcDev->close();  // throws nothing, no error handling
    }

    // destruct the objects in the controller info vector and remove all elements
    //
    this->controller.clear();

    this->devlist.clear();   // no devices open
    this->numdev = 0;        // no devices open
    return NO_ERROR;         // always succeeds
  }
  /** AstroCam::Interface::disconnect_controller ******************************/


  /** AstroCam::Interface::configure_controller *******************************/
  /**
   * @fn     configure_controller
   * @brief  callback function for frame received
   * @param  
   * @return none
   *
   */
  long Interface::configure_controller() {
    std::string function = "AstroCam::Interface::configure_controller";
    std::stringstream message;
    int applied=0;
    long error = NO_ERROR;

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      if (this->config.param[entry].compare(0, 16, "DEFAULT_FIRMWARE")==0) {
        // tokenize each firmware entry because it contains a controller dev number and a filename
        //
        std::vector<std::string> tokens;
        int size = Tokenize( this->config.arg[entry], tokens, " \t" );
        if (size != 2) {
          message << "ERROR: bad entry for DEFAULT_FIRMWARE: " << this->config.arg[entry] << ": expected (devnum filename)";
          logwrite(function, message.str());
          error = ERROR;
          continue;
        }
        else {
          this->camera_info.firmware[ parse_val(tokens.at(0)) ] = tokens.at(1);
          applied++;
        }
      }

      if (this->config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->common.imdir( config.arg[entry] );
        applied++;
      }

      if (this->config.param[entry].compare(0, 6, "BASENAME")==0) {
        this->common.basename( config.arg[entry] );
        applied++;
      }

    }

    message.str("");
    if (applied==0) {
      message << "ERROR: ";
      error = ERROR;
    }
    message << "applied " << applied << " configuration lines to controller";
    logwrite(function, message.str());
    return error;
  }
  /** AstroCam::Interface::configure_controller *******************************/


  /** AstroCam::Interface::native *********************************************/
  /**
   * @fn     native
   * @brief  send a 3-letter command to the Leach controller
   * @param  cmdstr, string containing command and arguments
   * @return 0 on success, 1 on error
   *
   */
  long Interface::native(std::string cmdstr) {
    std::string function = "AstroCam::Interface::native";
    std::stringstream message;
    std::vector<std::string> tokens;

    if ( cmdstr.empty() ) {
      logwrite(function, "ERROR: missing command");
      return( ERROR );
    }

    std::vector<uint32_t> cmd;      // this vector will contain the cmd and any arguments
    uint32_t c0, c1, c2;

    Tokenize(cmdstr, tokens, " ");

    int nargs = tokens.size() - 1;  // one token is for the command, this is number of args

    // max 4 arguments
    //
    if (nargs > 4) {
      message.str(""); message << "ERROR: too many arguments: " << nargs << " (max 4)";
      logwrite(function, message.str());
      return(1);
    }

    try {
      // first token is command and require a 3-letter command
      //
      if (tokens.at(0).length() != 3) {
        message.str(""); message << "ERROR: bad command " << tokens.at(0) << ": native command requires 3 letters";
        logwrite(function, message.str());
        return(1);
      }

      // Change the 3-letter (ASCII) command into hex byte representation
      // and push it into cmd vector.
      //
      c0  = (uint32_t)tokens.at(0).at(0); c0 = c0 << 16;
      c1  = (uint32_t)tokens.at(0).at(1); c1 = c1 <<  8;
      c2  = (uint32_t)tokens.at(0).at(2);

      cmd.push_back( c0 | c1 | c2 );

      // convert each arg into a number and push into cmd vector
      //
      tokens.erase( tokens.begin() );                 // first, erase the command from the tokens
      for ( auto tok : tokens ) {
        cmd.push_back( (uint32_t)parse_val( tok ) );  // then push each converted arg into cmd vector
      }

      // Log the complete command (with arg list) that will be sent
      //
      message.str("");   message << "sending command:"
                                 << std::setfill('0') << std::setw(2) << std::uppercase << std::hex;
      for (auto arg : cmd) message << " 0x" << arg;
      logwrite(function, message.str());
    }
    catch(std::out_of_range &) {  // should be impossible but here for safety
      message.str(""); message << "ERROR: unable to parse command ";
      for (auto check : tokens) message << check << " ";
      message << ": out of range";
      logwrite(function, message.str());
      return(ERROR);
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR forming command: " << e.what();
      logwrite(function, message.str());
      return(ERROR);
    }
    catch(...) { logwrite(function, "unknown error forming command"); return(ERROR); }

    // Send the command to each connected device via a separate thread
    //
    {  // begin local scope
    std::vector<std::thread> threads;       // create a local scope vector for the threads
    for (auto dev : this->devlist) {        // spawn a thread for each device in devlist
      try {
        std::thread thr( std::ref(AstroCam::Interface::dothread_native), std::ref(this->controller.at(dev)), cmd );
        threads.push_back(std::move(thr));  // push the thread into a vector
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: unable to find device " << dev << " in list: {";
        for (auto check : this->devlist) message << check << " ";
        message << "}";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(const std::exception &e) {
        message.str(""); message << "ERROR threading command: " << e.what();
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error threading command to controller"); return(ERROR); }
    }

    try {
      for (std::thread & thr : threads) {   // loop through the vector of threads
        if ( thr.joinable() ) thr.join();   // if thread object is joinable then join to this function. (not to each other)
      }
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR joining threads: " << e.what();
      logwrite(function, message.str());
      return(ERROR);
    }
    catch(...) { logwrite(function, "unknown error joining threads"); return(ERROR); }

    threads.clear();                        // deconstruct the threads vector
    }  // end local scope

    // Log the return values
    // TODO need a way to send these back to the calling function
    //
    for (auto dev : devlist) {
      try {
        message.str(""); message << this->controller.at(dev).devname << " returns " << this->controller.at(dev).retval;
        logwrite(function, message.str());
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: unable to find device " << dev << " in list: {";
        for (auto check : this->devlist) message << check << " ";
        message << "}";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error looking for return values"); return(ERROR); }
    }
    return ( NO_ERROR );
  }
  /** AstroCam::Interface::native *********************************************/


  /** AstroCam::Interface::dothread_native ************************************/
  /**
   * @fn     dothread_native
   * @brief  run in a thread to actually send the command
   * @param  devnum, integer device number for logging only
   * @param  controller, controller_info struct member element
   * @param  cmd, vector containing command and args
   * @return none
   *
   */
  void Interface::dothread_native( controller_info &controller, std::vector<uint32_t> cmd ) {
    std::string function = "AstroCam::Interface::dothread_native";
    std::stringstream message;

    try {
      // ARC_API now uses an initialized_list object for the TIM_ID, command, and arguments.
      // The list object must be instantiated with a fixed size at compile time.
      //
      if (cmd.size() == 1) controller.retval = controller.pArcDev->command( { TIM_ID, cmd.at(0) } );
      else
      if (cmd.size() == 2) controller.retval = controller.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1) } );
      else
      if (cmd.size() == 3) controller.retval = controller.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1), cmd.at(2) } );
      else
      if (cmd.size() == 4) controller.retval = controller.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1), cmd.at(2), cmd.at(3) } );
      else
      if (cmd.size() == 5) controller.retval = controller.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1), cmd.at(2), cmd.at(3), cmd.at(4) } );
      else {
        message.str(""); message << "ERROR: invalid number of command arguments: " << cmd.size();
        logwrite(function, message.str());
        controller.retval = 0xDEAD;
      }
    }
    catch(const std::runtime_error &e) {
      message.str(""); message << "ERROR: " << controller.devname << ": " << e.what();
      logwrite(function, message.str());
      return;
    }
    catch(std::out_of_range &) {  // impossible
      logwrite(function, "ERROR: indexing command argument");
      return;
    }
    catch(...) {
      message.str(""); message << "unknown error sending command to " << controller.devname;
      logwrite(function, message.str());
      return;
    }
  }
  /** AstroCam::Interface::dothread_native ************************************/


  /** AstroCam::Interface::get_parameter **************************************/
  /**
   * @fn     get_parameter
   * @brief  
   * @param  
   * @return 
   *
   */
  long Interface::get_parameter(std::string parameter, std::string &retstring) {
    std::string function = "AstroCam::Interface::get_parameter";
    logwrite(function, "error: invalid command for this controller");
    return(ERROR);
  }
  /** AstroCam::Interface::get_parameter **************************************/


  /** AstroCam::Interface::set_parameter **************************************/
  /**
   * @fn     set_parameter
   * @brief  
   * @param  
   * @return 
   *
   */
  long Interface::set_parameter(std::string parameter) {
    std::string function = "AstroCam::Interface::set_parameter";
    logwrite(function, "error: invalid command for this controller");
    return(ERROR);
  }
  /** AstroCam::Interface::set_parameter **************************************/


  /** AstroCam::Interface::nframes ********************************************/
  /**
   * @fn     nframes
   * @brief  
   * @param  
   * @return 
   *
   */
  long Interface::access_nframes(std::string valstring) {
    std::string function = "AstroCam::Interface::nframes";
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

    this->nfpseq  = parse_val(tokens.at(1));         // requested nframes is nframes/sequence
    this->nframes = this->nfpseq * this->nsequences; // number of frames is (frames/sequence) x (sequences)
    std::stringstream snf;
    snf << "SNF " << this->nframes;                  // SNF sets total number of frames (Y:<N_FRAMES) on timing board

    message.str(""); message << "sending " << snf.str();
    logwrite(function, message.str());

    if ( this->native(snf.str()) != 0x444F4E ) return -1;

    std::stringstream fps;
    fps << "FPS " << nfpseq;            // FPS sets number of frames per sequence (Y:<N_SEQUENCES) on timing board

    message.str(""); message << "sending " << fps.str();
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

    message.str(""); message << "sending " << fpb.str();
    logwrite(function, message.str());

    if ( this->native(fpb.str()) == 0x444F4E ) return 0; else return -1;

    return 0;
  }
  /** AstroCam::Interface::nframes ********************************************/


  /** AstroCam::Interface::expose *********************************************/
  /**
   * @fn     expose
   * @brief  initiate an exposure
   * @param  nseq_in string, if set becomes the number of sequences
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::expose(std::string nseq_in) {
    std::string function = "AstroCam::Interface::expose";
    std::stringstream message;

    // check image size
    //
    int rows = this->get_rows();
    int cols = this->get_cols();
    if (rows < 1 || cols < 1) {
      message.str(""); message << "error: image size must be non-zero: rows=" << rows << " cols=" << cols;
      logwrite(function, message.str());
      return(ERROR);
    }

    // check buffer size
    // need allocation for at least 2 frames if nframes is greater than 1
    //
    int bufsize = this->get_bufsize();
    if ( bufsize < (int)( (this->nframes>1?2:1) * rows * cols * sizeof(unsigned short) ) ) {  // TODO type check bufsize: is it big enough?
      message.str(""); message << "error: insufficient buffer size (" << bufsize 
                               << " bytes) for " << this->nframes << " frame" << (this->nframes==1?"":"s")
                               << " of " << rows << " x " << cols << " pixels";
      logwrite(function, message.str());
      message.str(""); message << "minimum buffer size is " << (this->nframes>1?2:1) * rows * cols * sizeof(unsigned short) << " bytes";
      logwrite(function, message.str());
      return(ERROR);
    }

    // Start the exposure here
    //
    try {
      bool abort=false;
      for (auto dev : this->devlist) {
        this->controller.at(dev).pArcDev->expose(this->camera_info.exposure_time, rows, cols, abort, this->controller.at(dev).callback, true);
      }
    }
    catch (const std::exception &e) {
      message.str(""); message << "ERROR: " << e.what();
      logwrite(function, message.str());
      return( ERROR );
    }
    catch(...) { logwrite(function, "unknown error"); return(ERROR); }

    return( NO_ERROR );
  }
  /** AstroCam::Interface::expose *********************************************/


  /** AstroCam::Interface::load_firmware **************************************/
  /**
   * @fn     load_firmware
   * @brief  load firmware (.lod) into one or all controller timing boards
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This function is overloaded
   *
   * This version is when no parameter is specified, in which case try to
   * load the default lodfiles specified in the config file. To do that,
   * a string is built and the other load_firmware(std::string) is called.
   *
   */
  long Interface::load_firmware() {
    std::string function = "AstroCam::Interface::load_firmware";

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      return(ERROR);
    }

    long error=NO_ERROR;

    // Loop through all of the default firmware entries from the configfile
    //
    for (auto fw = this->camera_info.firmware.begin(); fw != this->camera_info.firmware.end(); ++fw) {
      std::stringstream lodfilestream;
      lodfilestream << fw->first << " " << fw->second;

      // Call load_firmware with the built up string.
      // If it returns an error then set error flag to return to the caller.
      //
      if (this->load_firmware(lodfilestream.str()) == ERROR) error = ERROR;
    }
    return(error);
  }
  /** AstroCam::Interface::load_firmware **************************************/


  /** AstroCam::Interface::load_firmware **************************************/
  /**
   * @fn     load_firmware
   * @brief  load firmware (.lod) into one or all controller timing boards
   * @param  timlodfile, string may contain a controller number
   * @return ERROR or NO_ERROR
   *
   * This function is overloaded
   *
   * The string passed in may contain a space-separated integer to indicate
   * a specific controller to load. In other words:
   *
   * "filename"     -> load filename into all connected devices
   * "0 filename"   -> load filename into device 0
   * "1 3 filename" -> load filename into devices 1 and 3
   *
   */
  long Interface::load_firmware(std::string timlodfile) {
    std::string function = "AstroCam::Interface::load_firmware";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::vector<uint32_t> selectdev;
    struct stat st;
    long error = ERROR;

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      return(ERROR);
    }

    // check the provided timlodfile string
    //
    if (timlodfile.empty()) {
      logwrite(function, "ERROR: no filename provided");
      return( ERROR );
    }

    // Tokenize the input string to separate out the filename from
    // any specified controller numbers.
    //
    Tokenize(timlodfile, tokens, " ");

    // Need at least one token
    //
    if (tokens.size() < 1) {
      logwrite(function, "ERROR: too few arguments");
      return( ERROR );
    }

    // Can't have more tokens than the number of controllers + 1
    // The +1 is for the file name.
    //
    if ((int)tokens.size() > this->numdev+1) {
      logwrite(function, "ERROR: too many arguments");
      return( ERROR );
    }

    // If there's only one token then it's the lodfile and load
    // into all controllers in the devlist.
    //
    if (tokens.size() == 1) {
      for (auto dev : devlist) {
        selectdev.push_back( dev );                        // build selectdev vector from all connected controllers
      }
    }

    // If there's more than one token then there are 1 or more dev numbers
    // in the string. Grab those and put them into a vector.
    // The last token must be the filename.
    //
    if (tokens.size() > 1) {
      for (uint32_t n = 0; n < (tokens.size()-1); n++) {   // tokens.size() - 1 because the last token must be the filename
        selectdev.push_back( (uint32_t)parse_val( tokens.at(n) ) );
      }
      timlodfile = tokens.at( tokens.size() - 1 );         // the last token must be the filename
    }

    // Finally, make sure the file exists before trying to load it
    //
    if (stat(timlodfile.c_str(), &st) != 0) {
      message.str(""); message << "error: " << timlodfile << " does not exist";
      logwrite(function, message.str());
    }
    else {
      // Run through the selectdev vector, spawning threads to do the load for each.
      //
      std::vector<std::thread> threads;               // create a local scope vector for the threads
      for (auto dev : selectdev) {                    // spawn a thread for each device in the selectdev list
        try {
          if (this->controller.at(dev).connected) {   // but only if connected
            std::thread thr( std::ref(AstroCam::Interface::dothread_load), std::ref(this->controller.at(dev)), timlodfile );
            threads.push_back ( std::move(thr) );     // push the thread into the local vector
          }
        }
        catch(std::out_of_range &) {
          message.str(""); message << "ERROR: unable to find device " << dev << " in list: {";
          for (auto check : selectdev) message << check << " ";
          message << "}";
          logwrite(function, message.str());
          return(ERROR);
        }
        catch(const std::exception &e) {
          message.str(""); message << "ERROR threading command: " << e.what();
          logwrite(function, message.str());
          return(ERROR);
        }
        catch(...) { logwrite(function, "unknown error threading command to controller"); return(ERROR); }
      }

      try {
        for (std::thread & thr : threads) {           // loop through the vector of threads
          if ( thr.joinable() ) thr.join();           // if thread object is joinable then join to this function (not to each other)
        }
      }
      catch(const std::exception &e) {
        message.str(""); message << "ERROR joining threads: " << e.what();
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error joining threads"); return(ERROR); }

      threads.clear();                                // deconstruct the threads vector

      // For now, log the return values after all threads complete
      // TODO
      //
      for (auto dev : selectdev) {
        message.str(""); message << this->controller.at(dev).devname << " returns " << this->controller.at(dev).retval;
        logwrite(function, message.str());
      } 
    }

    return( error );
  }
  /** AstroCam::Interface::load_firmware **************************************/


  /** AstroCam::Interface::dothread_load **************************************/
  /**
   * @fn     dothread_load
   * @brief  run in a thread to actually perform the load
   * @param  devnum, integer device number for logging only
   * @param  pArcDev, pointer to CArcDevice
   * @param  timlodfile, string of lodfile name to load
   * @return none
   *
   * loadControllerFile() doesn't return a value but throws std::runtime_error on error.
   *
   * The retval for each device in the contoller info structure will be set here,
   * to ERR on exception or to DON if no exception is thrown.
   *
   */
  void Interface::dothread_load(controller_info &controller, std::string timlodfile) {
    std::string function = "AstroCam::Interface::dothread_load";
    std::stringstream message;

    try {
      controller.pArcDev->loadControllerFile( timlodfile );  // Do the load here
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR: " << controller.devname << ": " << e.what();
      logwrite(function, message.str());
      controller.retval = ERR;
      controller.firmwareloaded = false;
      return;
    }
    catch(...) {
      message.str(""); message << "unknown error loading firmware for " << controller.devname;
      logwrite(function, message.str());
      controller.retval = ERR;
      controller.firmwareloaded = false;
      return;
    }
    message.str(""); message << "devnum " << controller.devnum << ": firmware loaded";
    logwrite(function, message.str());
    controller.retval = DON;
    controller.firmwareloaded = true;
  }
  /** AstroCam::Interface::dothread_load **************************************/


  /** AstroCam::Interface::buffer *********************************************/
  /**
   * @fn     buffer
   * @brief  set/get mapped image buffer
   * @param  
   * @param  
   * @param  
   * @return 
   *
   */
  long Interface::buffer(std::string size_in) {
    std::string function = "AstroCam::Interface::buffer";
    std::stringstream message;
    uint32_t try_bufsize=0;

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      return(ERROR);
    }

    // If no buffer size specified then nothing to do here  //TODO should return the current setting
    //
    if (size_in.empty()) {
      logwrite(function, "ERROR: no buffer size specified");
      return(ERROR);
    }

    // Tokenize size_in string and set buffer size accordingly --
    // size_in string can contain 1 value to allocate that number of bytes
    // size_in string can contain 2 values to allocate a buffer of rows x cols
    // Anything other than 1 or 2 tokens is invalid.
    //
    std::vector<std::string> tokens;
    switch( Tokenize( size_in, tokens, " " ) ) {
      case 1:  // allocate specified number of bytes
               try_bufsize = (uint32_t)parse_val( tokens.at(0) );
               break;
      case 2:  // allocate (row x col) bytes
               try_bufsize = (uint32_t)parse_val(tokens.at(0)) * (uint32_t)parse_val(tokens.at(1)) * sizeof(uint16_t);
               break;
      default: // bad
               message.str(""); message << "ERROR: invalid arguments: " << size_in << ": expected bytes or rows cols";
               logwrite(function, message.str());
               return(ERROR);
               break;
    }

    // For each connected device, try to re-map the requested buffer size
    //
    for (auto dev : this->devlist) {
      try {
        this->controller.at(dev).pArcDev->reMapCommonBuffer( try_bufsize );
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: unable to find device " << dev << " in list: {";
        for (auto check : this->devlist) message << check << " ";
        message << "}";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(const std::exception &e) {
        message.str(""); message << "ERROR: device number " << dev << ": " << e.what();
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error mapping memory"); return(ERROR); }

      this->bufsize = try_bufsize;
    }
    return(NO_ERROR);
  }
  /** AstroCam::Interface::buffer *********************************************/


  /**************** AstroCam::Interface::set_camera_mode **********************/
  /**
   * @fn     set_camera_mode
   * @brief  
   * @param  none
   * @return 
   *
   */
  long Interface::set_camera_mode(std::string mode) {
    std::string function = "AstroCam::Interface::set_camera_mode";
    std::stringstream message;
    logwrite(function, "ERROR: not implemented");
    return( ERROR );
  }
  /**************** AstroCam::Interface::set_camera_mode **********************/


  /**************** AstroCam::Interface::write_frame **************************/
  /**
   * @fn     write_frame
   * @brief  creates a FITS_file object to write the image_data buffer to disk
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * A FITS_file object is created here to write the data. This object MUST remain
   * valid while any (all) threads are writing data, so the write_data function
   * will keep track of threads so that it doesn't terminate until all of its 
   * threads terminate.
   *
   * The camera_info class was copied into fits_info when the exposure was started,  //TODO I've un-done this.
   * so use fits_info from here on out.                                              //TODO Don't use fits_info right now.
   *                                                                                 //TODO Only using camera_info
   */
  long Interface::write_frame() {
    std::string function = "AstroCam::Interface::write_frame";
    std::stringstream message;
    logwrite(function, "ERROR: not implemented");
    return( ERROR );
  }
  /**************** AstroCam::Interface::write_frame **************************/


  /**************** AstroCam::Interface::exptime ******************************/
  /**
   * @fn     exptime
   * @brief  set/get the exposure time
   * @param  string
   * @return ERROR or NO_ERROR
   *
   * This function calls "set_parameter()" and "get_parameter()" using
   * the "exptime" parameter (which must already be defined in the ACF file).
   *
   */
  long Interface::exptime(std::string exptime_in, std::string &retstring) {
    std::string function = "AstroCam::Interface::exptime";
    std::stringstream message;
    long ret=NO_ERROR;

    // If an exposure time was passed in then
    // try to convert it (string) to an integer
    //
    if ( ! exptime_in.empty() ) {
      try {
        this->camera_info.exposure_time = std::stoi( exptime_in );
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR: unable to convert exposure time: " << exptime_in << " to integer";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR: exposure time " << exptime_in << " outside integer range";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error setting exposure time"); return(ERROR); }

      // Now send it to the controller via the SET command.
      // Even though exptime started as a string and it's tempting to use that string here,
      // instead build the command from the stoi version to ensure that the value sent is 
      // the same as that stored now in this->camera_info.exposure_time
      //
      std::stringstream cmd;
      cmd << "SET " << this->camera_info.exposure_time;
      ret = this->native( cmd.str() );
    }
    retstring = std::to_string( this->camera_info.exposure_time );
    message.str(""); message << "exposure time is " << retstring << " msec";
    logwrite(function, message.str());
    return(ret);
  }
  /**************** AstroCam::Interface::exptime ******************************/


  /**************** AstroCam::Interface::geometry *****************************/
  /**
   * @fn     geometry
   * @brief  set/get geometry
   * @param  args contains: rows cols
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::geometry(std::string args, std::string &retstring) {
    std::string function = "AstroCam::Interface::geometry";
    std::stringstream message;
    std::stringstream ret;

    // If geometry was passed in then get each value and try to convert them to integers
    //
    if ( ! args.empty() ) {
      try {
        std::vector<std::string> tokens;
        int toks = Tokenize( args, tokens, " " );     // Tokenize the input string
        if ( toks == 2 ) {                            // Must receive two tokens
          this->set_rows( std::stoi( tokens[0] ) );   // First token is rows
          this->set_cols( std::stoi( tokens[1] ) );   // Second token is cols
        }
        else {                                        // Error if not two tokens
          message.str(""); message << "ERROR: expected 2 values (rows cols) but received " << toks << ": " << args;
          logwrite(function, message.str());
          return( ERROR );
        }
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR: unable to convert one or more values to integer: " << args;
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR: one or more values outside integer range: " << args;
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error setting geometry"); return(ERROR); }
    }

    // Form the return string from the current rows cols
    //
    ret << this->get_rows() << " " << this->get_cols();
    retstring = ret.str();

    return( NO_ERROR );
  }
  /**************** AstroCam::Interface::geometry *****************************/


  /**************** AstroCam::Interface::bias *********************************/
  /**
   * @fn     bias
   * @brief  set a bias
   * @param  args contains: module, channel, bias
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::bias(std::string args, std::string &retstring) {
    std::string function = "AstroCam::Interface::bias";
    std::stringstream message;
    logwrite(function, "ERROR: not implemented");
    return( ERROR );
  }
  /**************** AstroCam::Interface::bias *********************************/


  /**************** AstroCam::Interface::handle_frame *************************/
  /**
   * @fn     bias
   * @brief  set a bias
   * @param  args contains: module, channel, bias
   * @return ERROR or NO_ERROR
   *
   */
  void Interface::handle_frame(uint32_t uiFrameCount, void* pBuffer) {
    std::string function = "AstroCam::Interface::handle_frame";
    std::stringstream message;
    logwrite(function, "ERROR: not implemented");
    return;
  }
  /**************** AstroCam::Interface::handle_frame *************************/

}
