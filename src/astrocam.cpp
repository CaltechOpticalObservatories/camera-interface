#include "astrocam.h"

// The main server object is instantiated in src/server.cpp and
// defined extern here so that static functions can access it. The
// static functions are run in std::threads which means class objects
// are otherwise be unavailable to them. The interface class is accessible
// through this because Camera::Server server inherits AstroCam::Interface.
//
#include "server.h"
extern Camera::Server server;

namespace AstroCam {

  /** AstroCam::Callback::exposeCallback **************************************/
  /**
   * @fn     exposeCallback
   * @brief  called by CArcDevice::expose() during the exposure
   * @param  devnum, device number passed to API on expose
   * @param  uiElapsedTime, actually number of millisec remaining
   * @return none
   *
   * This is the callback function invoked by the ARC API,
   * arc::gen3::CArcDevice::expose() during the exposure.
   * After sending the expose (SEX) command, the API polls the controller
   * using the RET command.
   *
   * TODO presumably tie this in to the GUI, or set a global that the CLI can access
   *
   */
  void Callback::exposeCallback( int devnum, std::uint32_t uiElapsedTime ) {
    std::string function = "AstroCam::Callback::exposeCallback";
    std::stringstream message;
#ifdef LOGLEVEL_DEBUG
    message <<"*** [DEBUG] elapsed exposure time [" << devnum << "] = " << uiElapsedTime;
    logwrite(function, message.str());
#endif
    if (uiElapsedTime==0) {
      message.str(""); message << server.controller.at(devnum).devname << " exposure complete";
      logwrite(function, message.str());
    }
  }
  /** AstroCam::Callback::exposeCallback **************************************/


  /** AstroCam::Callback::readCallback ****************************************/
  /**
   * @fn     readCallback
   * @brief  called by CArcDevice::expose() during readout
   * @param  devnum, device number passed to API on expose
   * @param  uiPixelCount, number of pixels read ( getPixelCount() )
   * @return none
   *
   * This is the callback function invoked by the ARC API,
   * arc::gen3::CArcDevice::expose() when bInReadout is true.
   *
   * TODO presumably tie this in to the GUI, or set a global that the CLI can access
   *
   */
  void Callback::readCallback( int devnum, std::uint32_t uiPixelCount ) {
#ifdef LOGLEVEL_DEBUG
    std::string function = "AstroCam::Callback::readCallback";
    std::stringstream message;
    message << "*** [DEBUG] pixel count [" << devnum << "] = " << uiPixelCount;
    logwrite(function, message.str());
#endif
  }
  /** AstroCam::Callback::readCallback ****************************************/


  /** AstroCam::Callback::frameCallback ***************************************/
  /**
   * @fn     frameCallback
   * @brief  called by CArcDevice::expose() when a frame has been received
   * @param  devnum, device number passed to API on expose
   * @param  fpbcount, frames per buffer count ( uiFPBCount ) wraps to 0 at FPB
   * @param  fcount, actual frame counter ( uiPCIFrameCount = getFrameCount() )
   * @param  rows, image rows ( uiRows )
   * @param  cols, image columns ( uiCols )
   * @param  buffer, pointer to PCI image buffer
   * @return none
   *
   * This is the callback function invoked by the ARC API,
   * arc::gen3::CArcDevice::expose() when a new frame is received.
   * Spawn a separate thread to handle the incoming frame.
   *
   * TODO presumably tie this in to the GUI, or set a global that the CLI can access
   *
   */
  void Callback::frameCallback( int devnum, std::uint32_t fpbcount, std::uint32_t fcount, std::uint32_t rows, std::uint32_t cols, void* buffer ) {
#ifdef LOGLEVEL_DEBUG
    std::string function = "AstroCam::Callback::frameCallback";
    std::stringstream message;
    message << "*** [DEBUG] spawning handle_frame thread: devnum=" << devnum << " frame=" << fcount 
            << " buffer=" << std::setfill('0') << std::hex << buffer;
    logwrite(function, message.str());
#endif
    server.add_framethread();  // framethread_count is incremented because a thread has been added
    std::thread( std::ref(AstroCam::Interface::handle_frame), devnum, fpbcount, fcount, buffer ).detach();
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
    this->nframes = 1;
    this->nfpseq = 1;
    this->useframes = true;
    this->framethreadcount = 0;
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
        this->controller.at(dev).pCallback = pCB;          // set the pointer to this object in the public vector
        this->controller.at(dev).devnum = dev;             // device number
        this->controller.at(dev).devname = pdevList[dev];  // device name
        this->controller.at(dev).connected = false;        // not yet connected
        this->controller.at(dev).firmwareloaded = false;   // no firmware loaded

        // The CameraFits class holds the Common::Information class and the FITS_file class,
        // as well as wrappers for calling the functions inside the FITS_file class.
        // Then a vector of CameraFits class objects is created, with one element for each ARC device.
        // This is where that vector is pushed.
        //
        CameraFits cam;                                    // create a CameraFits object
        this->camera.push_back( cam );                     // push it into the vector
        this->camera.at(dev).set_devnum( dev );            // TODO is this useful?

        FITS_file* pFits = new FITS_file();                // create a pointer to a FITS_file class object
        this->camera.at(dev).pFits = pFits;                // set the pointer to this object in the public vector

#ifdef LOGLEVEL_DEBUG
        message.str("");
        message << "*** [DEBUG] pointers for dev " << dev << ": "
                << " pArcDev=" << std::hex << this->controller.at(dev).pArcDev 
                << " pCB="     << std::hex << this->controller.at(dev).pCallback
                << " pFits="   << std::hex << this->camera.at(dev).pFits;
        logwrite(function, message.str());
#endif
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

    // Now remove the marked devices (those not connected) 
    // by erasing them from the this->devlist STL map.
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
   * @brief  perform initial configuration of controller from .cfg file
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * Called automatically by main() when the server starts up.
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
          try {
            this->common.firmware[ parse_val(tokens.at(0)) ] = tokens.at(1);
            applied++;
          }
          catch(std::out_of_range &) {  // should be impossible but here for safety
            logwrite(function, "ERROR configuring DEFAULT_FIRMWARE: requested tokens out of range");
            error = ERROR;
          }
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
   * @return NO_ERROR on success, ERROR on error
   *
   */
  long Interface::native(std::string cmdstr) {
    std::string retstring;
    return this->native(cmdstr, retstring);
  }
  long Interface::native(std::string cmdstr, std::string &retstring) {
    std::string function = "AstroCam::Interface::native";
    std::stringstream message;
    std::vector<std::string> tokens;

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      return(ERROR);
    }

    // If no command passed then nothing to do here
    //
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
      return(ERROR);
    }

    try {
      // first token is command and require a 3-letter command
      //
      if (tokens.at(0).length() != 3) {
        message.str(""); message << "ERROR: bad command " << tokens.at(0) << ": native command requires 3 letters";
        logwrite(function, message.str());
        return(ERROR);
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

    // Check to see if all retvals are the same by comparing them all to the first.
    //
    std::uint32_t check_retval;
    try {
      check_retval = this->controller.at(0).retval;    // save the first one in the controller vector
    }
    catch(std::out_of_range &) {
      logwrite(function, "ERROR: no device found. Is the controller connected?");
      return(ERROR);
    }

    bool allsame = true;
    for (auto dev : devlist) {
      try {
        if (this->controller.at(dev).retval != check_retval) {
          allsame = false;
        }
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

    // If all the return values are equal then return only one value...
    //
    if ( allsame ) {
      this->retval_to_string( check_retval, retstring );        // this sets retstring = to_string( retval )
    }

    // ...but if any retval is different from another then we have to report each one.
    //
    else {
      std::stringstream rs;
      for (auto dev : devlist) {
        try {
          this->retval_to_string( this->controller.at(dev).retval, retstring );          // this sets retstring = to_string( retval )
          rs << std::dec << this->controller.at(dev).devnum << ":" << retstring << " ";  // build up a stringstream of each controller's reply
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
      retstring = rs.str();  // re-use retstring to contain all of the replies
    }

    // Log the return values
    // TODO need a way to send these back to the calling function
    //
    for (auto dev : devlist) {
      try {
        message.str(""); message << this->controller.at(dev).devname << " returns " << std::dec << this->controller.at(dev).retval;
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


  /** AstroCam::Interface::retval_to_string ***********************************/
  /**
   * @fn     retval_to_string
   * @brief  private function to convert ARC return values (long) to string
   * @param  retval, input long int retval
   * @param  retstring, reference to a string for return value
   * @return none
   *
   * In addition to converting longs to string, if the retval is a particular
   * commonly used code, then set the ASCII characters for those codes,
   * such as DON, ERR, etc. instead of returning a string value of their
   * numeric counterpart.
   *
   */
  void Interface::retval_to_string( std::uint32_t retval, std::string& retstring ) {
    // replace some common values with their ASCII equivalents
    //
    if ( retval == 0x00455252 ) { retstring = "ERR";  }
    else
    if ( retval == 0x00444F4E ) { retstring = "DON";  }
    else
    if ( retval == 0x544F5554 ) { retstring = "TOUT"; }
    else
    if ( retval == 0x524F5554 ) { retstring = "ROUT"; }
    else
    if ( retval == 0x48455252 ) { retstring = "HERR"; }
    else
    if ( retval == 0x00535952 ) { retstring = "SYR";  }
    else
    if ( retval == 0x00525354 ) { retstring = "RST";  }
    else
    if ( retval == 0x00434E52 ) { retstring = "CNR";  }

    // otherwise just convert the numerica value to a string
    //
    else {
      retstring = std::to_string( retval );
    }
  }
  /** AstroCam::Interface::retval_to_string ***********************************/


  /** AstroCam::Interface::dothread_expose ************************************/
  /**
   * @fn     dothread_expose
   * @brief  run in a thread to actually send the command
   * @param  controller, reference to controller_info struct member element
   * @param  camera, reference ro CameraFits class object element
   * @return none
   *
   */
  void Interface::dothread_expose( controller_info &controller, CameraFits &camera ) {
    std::string function = "AstroCam::Interface::dothread_expose";
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message.str("");
    message << "*** [DEBUG] controller.devnum=" << controller.devnum 
            << " .devname=" << controller.devname << " camera.image_size=" << camera.info.image_size
            << " shutterenable=" << server.common.shutterenable;
    logwrite(function, message.str());
#endif

    // Start the exposure here.
    // A callback function triggered by the ARC API will perform the writing of frames to disk.
    //
    try {
      // get system time just before the actual expose() call
      //
      camera.info.start_time = get_system_time();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss

      // Send the actual command to start the exposure.
      // This API call will send the SEX command to trigger the exposure.
      // The devnum is passed in so that the Callback functions know which device they belong to.
      //
      controller.pArcDev->expose(controller.devnum, 
                                 camera.info.exposure_time, 
                                 camera.info.detector_pixels[0], 
                                 camera.info.detector_pixels[1], 
                                 server.common.abortstate, 
                                 controller.pCallback, 
                                 server.common.shutterenable);

      // write the exposure start time in the header
      //
      camera.pFits->add_user_key("EXPSTART", "STRING", camera.info.start_time, "exposure start time");
    }
    catch (const std::exception &e) {
      // arc::gen3::CArcDevice::expose() will throw an exception for an abort.
      // Look for the word "abort" in the exception message and log "ABORT"
      // instead of "ERROR".
      //
      std::string estring = e.what();
      message.str("");
      if ( estring.find("aborted") != std::string::npos ) {
        message << "ABORT: " << e.what();
      }
      else {
        message << "ERROR: " << e.what();
      }
      server.common.set_abortstate( true );
      logwrite(function, message.str());
      return;
    }
    catch(...) {
      logwrite(function, "unknown error calling pArcDev->expose(). forcing abort.");
      server.common.set_abortstate( true );
      return;
    }
  }
  /** AstroCam::Interface::dothread_expose ************************************/


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


  /** AstroCam::Interface::access_useframes ***********************************/
  /**
   * @fn     access_useframes
   * @brief  set or get the state of this->useframes
   * @param  useframes, reference to std::string
   * @return ERROR or NO_ERROR
   *
   * The argument is passed in by reference so that it can be used for the
   * return value of this->useframes, while the actual return value
   * indicates an error condition. If the reference contains an empty
   * string then this is a "get" operation. If the reference is not empty
   * then this is a "set" operation.
   *
   * this->useframes indicates whether or not the firmware supports frames,
   * true if it does, false if it does not. 
   *
   */
  long Interface::access_useframes(std::string& useframes) {
    std::string function = "AstroCam::Interface::access_useframes";
    std::stringstream message;
    std::vector<std::string> tokens;

    // If an empty string reference is passed in then that means to return
    // the current value using that reference.
    //
    if ( useframes.empty() ) {
      useframes = ( this->useframes ? "true" : "false" );  // set the return value here on empty
      message << "useframes is " << useframes;
      logwrite(function, message.str());
      return( NO_ERROR );
    }

    Tokenize(useframes, tokens, " ");

    if (tokens.size() != 1) {
      message.str(""); message << "error: expected 1 argument but got " << tokens.size();
      logwrite(function, message.str());
      useframes = ( this->useframes ? "true" : "false" );  // set the return value here on bad number of args
      return(ERROR);
    }

    // Convert to lowercase then compare against "true" and "false"
    // and set boolean class member and return value string.
    //
    std::transform( useframes.begin(), useframes.end(), useframes.begin(), ::tolower );

    if ( useframes.compare("true")  == 0 ) { this->useframes = true;  useframes = "true";  }
    else
    if ( useframes.compare("false") == 0 ) { this->useframes = false; useframes = "false"; }

    // If neither true nor false then log the error,
    // and set the return value to the current value.
    //
    else {
      message << "ERROR: unrecognized argument: " << useframes << ". Expected true or false.";
      logwrite(function, message.str());
      useframes = ( this->useframes ? "true" : "false" );  // set the return value here on invalid arg
      return( ERROR );
    }

    message << "useframes is " << useframes;
    logwrite(function, message.str());
    return( NO_ERROR );
  }
  /** AstroCam::Interface::access_useframes ***********************************/


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
      message.str(""); message << "error: expected 1 value but got " << tokens.size()-1;
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
    std::string nseqstr;
    long error;
    int nseq;

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

    // If nseq_in is not supplied then set nseq to 1
    //
    if ( nseq_in.empty() ) {
      nseqstr = "1";
      nseq=1;
    }
    else {                                                          // sequence argument passed in
      try {
        nseq = std::stoi( nseq_in );                                // test that nseq_in is an integer
        nseqstr = nseq_in;                                          // before trying to use it
        this->camera_info.extension = 0;
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR: unable to convert sequences: " << nseq_in << " to integer";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR: sequences " << nseq_in << " outside integer range";
        logwrite(function, message.str());
        return(ERROR);
      }
    }

    // TODO
    // Currently, astrocam doesn't use "modes" like Archon does -- do we need that?
    // For the time being, set a fixed bitpix here. Doesn't seem like a final
    // solution but need to set it someplace for now.
    //
    this->camera_info.detector_pixels[0] = this->get_rows();
    this->camera_info.detector_pixels[1] = this->get_cols();
    // ROI is the full detector
    this->camera_info.region_of_interest[0] = 1;
    this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
    this->camera_info.region_of_interest[2] = 1;
    this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
    // Binning factor (no binning)
    this->camera_info.binning[0] = 1;
    this->camera_info.binning[1] = 1;

    this->camera_info.bitpix = 16;
    error = this->camera_info.set_axes(SHORT_IMG);  // 16 bit image is short int

    // Clear the abort flag for a new exposure, in case it was previously set
    //
    this->common.set_abortstate( false );

    // Initialize framethread count -- this keeps track of threads
    // spawned by frameCallback, to make sure that all have been completed.
    //
    this->init_framethread_count();

    // Copy the userkeys database object into camera_info
    //
    this->camera_info.userkeys.keydb = this->userkeys.keydb;

    this->camera_info.start_time = get_system_time();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
    this->common.set_fitstime(this->camera_info.start_time);        // sets common.fitstime (YYYYMMDDHHMMSS) used for filename

    message.str(""); message << "starting exposure at " << camera_info.start_time;
    logwrite(function, message.str());

    // prepare the camera info class object for each controller
    //
    for (auto dev : this->devlist) {        // spawn a thread for each device in devlist
      try {

        // Initialize a frame counter for each device. 
        //
        this->camera.at(dev).init_framecount();

        // Copy the camera_info into each dev's info class object
        this->camera.at(dev).info = this->camera_info;

        this->camera.at(dev).alloc_workbuf();

        // then set the filename for this specific dev
        // Assemble the FITS filename.
        // If naming type = "time" then this will use this->fitstime so that must be set first.
        this->camera.at(dev).info.fits_name = this->common.get_fitsname( this->camera.at(dev).info.fits_name );

#ifdef LOGLEVEL_DEBUG
        message.str("");
        message << "*** [DEBUG] pointers for dev " << dev << ": "
                << " pArcDev="  << std::hex << this->controller.at(dev).pArcDev 
                << " pCB="      << std::hex << this->controller.at(dev).pCallback
                << " pFits="    << std::hex << this->camera.at(dev).pFits;
        logwrite(function, message.str());
#endif
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR: unable to find device " << dev << " in list: {";
        for (auto check : this->devlist) message << check << " ";
        message << "}";
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error creating fitsname for controller"); return(ERROR); }
    }

    // Spawn separate threads to call ARC_API's expose() on each connected device, in parallel
    //
    {  // begin local scope
    std::vector<std::thread> threads;       // create a local scope vector for the threads
    for (auto dev : this->devlist) {        // spawn a thread for each device in devlist
      try {

        // Open the fits file, ...
        //
        error = this->camera.at(dev).open_file();  // TODO error handling on this?

        // ... then spawn the expose thread.
        //
        std::thread thr( std::ref(AstroCam::Interface::dothread_expose), std::ref(this->controller.at(dev)), std::ref(this->camera.at(dev)) );
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
        message.str(""); message << "ERROR creating expose thread: " << e.what();
        logwrite(function, message.str());
        return(ERROR);
      }
      catch(...) { logwrite(function, "unknown error creating expose thread for controller"); return(ERROR); }
    }

    // Join the dothread_expose threads to this thread (not to each other)
    // in order to wait for them to complete.
    //
    try {
      for (std::thread & thr : threads) {   // loop through the vector of threads
        if ( thr.joinable() ) thr.join();   // if thread object is joinable then join to this function. (not to each other)
      }
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR joining expose threads: " << e.what();
      logwrite(function, message.str());
      return(ERROR);
    }
    catch(...) { logwrite(function, "unknown error joining expose threads"); return(ERROR); }

    threads.clear();                        // deconstruct the threads vector
    }  // end local scope

    // As frames are received, threads are created and a counter keeps track of them,
    // incrementing on creation and decrementing on destruction. Wait here for that
    // counter to be zero, indicating all the threads have completed.
    //
    while ( this->get_framethread_count() > 0 ) ;

    logwrite(function, "all frames written");

    // close the FITS file
    //
    for (auto dev : this->devlist) {
      try {
        this->camera.at(dev).close_file();
      }
      catch(std::out_of_range &) {
        message.str(""); message << "ERROR closing FITS file: unable to find device " << dev << " in list: {";
        for (auto check : this->devlist) message << check << " ";
        message << "}";
        logwrite(function, message.str());
        error = ERROR;
      }
      catch(...) { logwrite(function, "unknown error closing FITS file(s)"); }
    }

    logwrite( function, (error==ERROR ? "ERROR" : "complete") );

    return( error );
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
  long Interface::load_firmware(std::string &retstring) {
    std::string function = "AstroCam::Interface::load_firmware";

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      return(ERROR);
    }

    long error=NO_ERROR;

    // Loop through all of the default firmware entries found in the configfile
    // which were stored in an STL map so that
    //   fw->first is the devnumber, and
    //   fw->second is the firmware filename.
    // Use these to build a string of the form "<dev> <filename>" to pass to the
    // load_firmware() function to load each controller with the specified file.
    //
    for (auto fw = this->common.firmware.begin(); fw != this->common.firmware.end(); ++fw) {
      std::stringstream lodfilestream;
      lodfilestream << fw->first << " " << fw->second;

      // Call load_firmware with the built up string.
      // If it returns an error then set error flag to return to the caller.
      //
      if (this->load_firmware(lodfilestream.str(), retstring) == ERROR) error = ERROR;
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
  long Interface::load_firmware(std::string timlodfile, std::string &retstring) {
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

      // Check to see if all retvals are the same by comparing them all to the first.
      //
      std::uint32_t check_retval;
      try {
        check_retval = this->controller.at(0).retval;    // save the first one in the controller vector
      }
      catch(std::out_of_range &) {
        logwrite(function, "ERROR: no device found. Is the controller connected?");
        return(ERROR);
      }

      bool allsame = true;
      for (auto dev : selectdev) {
        try {
          if (this->controller.at(dev).retval != check_retval) {
            allsame = false;
          }
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

      // If all the return values are equal then report only NO_ERROR (if "DON") or ERROR (anything else)
      //
      if ( allsame ) {
        if (check_retval == DON) { error = NO_ERROR; }
        else                     { error = ERROR;    }
      }

      // ...but if any retval is different from another then we have to report each one.
      // This will automatically flag an error. The format will be
      // devnum:retval devnum:retval etc., concatenated together.
      //
      else {
        std::stringstream rs;
        for (auto dev : selectdev) {
          try {
            rs << this->controller.at(dev).devnum << ":";
            if (this->controller.at(dev).retval == DON) rs << "DON ";  // Return DON,
            else
            if (this->controller.at(dev).retval == ERR) rs << "ERR ";  // or ERR,
            else
            rs << "0x" << std::uppercase << std::hex << check_retval;  // or hex value if other.
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
        retstring = rs.str();
        error = ERROR;
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
   * @param  string size_in, string containing the buffer allocation info
   * @param  string &retstring, reference to a string for return values
   * @return ERROR or NO_ERROR
   *
   * This function uses the ARC API to allocate PCI/e buffer space for doing
   * the DMA transfers.
   *
   * Function returns only ERROR or NO_ERROR on error or success. The 
   * reference to retstring is used to return the size of the allocated
   * buffer.
   *
   */
  long Interface::buffer(std::string size_in, std::string &retstring) {
    std::string function = "AstroCam::Interface::buffer";
    std::stringstream message;
    uint32_t try_bufsize=0;

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      return(ERROR);
    }

    // If no buffer size specified then return the current setting
    //
    if (size_in.empty()) {
      retstring = std::to_string( this->bufsize );
      return(NO_ERROR);
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
      case 2:  // allocate (col x row) bytes
               try_bufsize = (uint32_t)parse_val(tokens.at(0)) * (uint32_t)parse_val(tokens.at(1)) * sizeof(uint16_t);
               break;
      default: // bad
               message.str(""); message << "ERROR: invalid arguments: " << size_in << ": expected bytes or cols rows";
               logwrite(function, message.str());
               return(ERROR);
               break;
    }

    // For each connected device, try to re-map the requested buffer size.
    // This is done sequentially for each device.
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
    retstring = std::to_string( this->bufsize );
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
   * @brief  writes the image_data buffer to disk
   * @param  int devnum, device number
   * @param  int fpbcount, frame number = also thread ID receiving the frame
   * @return ERROR or NO_ERROR
   *
   * This function is called by the handle_frame thread.
   *
   */
  long Interface::write_frame(int devnum, int fpbcount) {
    std::string function = "AstroCam::Interface::write_frame";
    std::stringstream message;
    long error=NO_ERROR;

    // Use devnum to identify which device has the frame,
    // and fpbcount to index the frameinfo STL map on that devnum,
    // and assign the pointer to that buffer to a local variable.
    //
    void* buf = this->controller.at(devnum).frameinfo[fpbcount].buf;

    message << this->controller.at(devnum).devname << " received frame " 
            << this->controller.at(devnum).frameinfo[fpbcount].framenum << " into buffer "
            << std::hex << this->controller.at(devnum).frameinfo[fpbcount].buf;
    logwrite(function, message.str());

    // Call the class' deinterlace and write functions
    // with the image buffer pointer cast to the appropriate type.
    //
    try {
      switch (this->camera_info.datatype) {
        case USHORT_IMG: {
          this->camera.at(devnum).deinterlace( (uint16_t *)buf );
          error = this->camera.at(devnum).write( (uint16_t *)buf );
          break;
        }
        case SHORT_IMG: {
          this->camera.at(devnum).deinterlace( (int16_t *)buf );
          error = this->camera.at(devnum).write( (int16_t *)buf );
          break;
        }
        case FLOAT_IMG: {
          this->camera.at(devnum).deinterlace( (uint32_t *)buf );
          error = this->camera.at(devnum).write( (uint32_t *)buf );
          break;
        }
        default:
          message.str("");
          message << "ERROR: unknown datatype: " << this->camera_info.datatype;
          logwrite(function, message.str());
          error = ERROR;
          break;
      }
      // A frame has been written for this device,
      // so increment the framecounter for devnum.
      //
      if (error == NO_ERROR) this->camera.at(devnum).increment_framecount();
    }
    catch (std::out_of_range &) {
      message.str(""); message << "ERROR: unable to find device " << devnum << " in list: {";
      for (auto check : this->devlist) message << check << " ";
      message << "}";
      logwrite(function, message.str());
      error = ERROR;
    }

//  this->increment_framecount();

    return( error );
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
   * @param  args contains: X Y (i.e. cols rows)
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::geometry(std::string args, std::string &retstring) {
    std::string function = "AstroCam::Interface::geometry";
    std::stringstream message;

    // If geometry was passed in then get each value and try to convert them to integers
    //
    if ( ! args.empty() ) {
      try {
        std::vector<std::string> tokens;
        int toks = Tokenize( args, tokens, " " );     // Tokenize the input string
        if ( toks == 2 ) {                            // Must receive two tokens
          this->set_cols( std::stoi( tokens[0] ) );   // First token is X dimension, cols
          this->set_rows( std::stoi( tokens[1] ) );   // Second token is Y dimension, rows
        }
        else {                                        // Error if not two tokens
          message.str(""); message << "ERROR: expected 2 values (cols rows) but received " << toks << ": " << args;
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

    std::stringstream cmd;
    long error = NO_ERROR;

    cmd.str(""); cmd << "WRM 0x400001 " << this->get_cols();
    if ( error == NO_ERROR && this->native( cmd.str() ) == ERROR ) error = ERROR;

    cmd.str(""); cmd << "WRM 0x400002 " << this->get_rows();
    if ( error == NO_ERROR && this->native( cmd.str() ) == ERROR ) error = ERROR;

    if (error == ERROR) logwrite(function, "ERROR: writing geometry to controller");

    // Form the return string from the current cols rows
    //
    std::stringstream ret;
    ret << this->get_cols() << " " << this->get_rows();
    retstring = ret.str();

    return( error );
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
   * @fn     handle_frame
   * @brief  process each frame received by frameCallback for any device
   * @param  fpbcount, frames per buffer count, returned from the ARC API
   * @param  fcount, number of frames read, returned from the ARC API
   * @param  *buffer, pointer to the PCI/e buffer, returned from the ARC API
   * @return none
   *
   * This function is static, spawned in a thread created by frameCallback()
   * which is the callback fired by the ARC API when a frame has been received.
   *
   */
  void Interface::handle_frame(int devnum, uint32_t fpbcount, uint32_t fcount, void* buffer) {
    std::string function = "AstroCam::Interface::handle_frame";
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message << "*** [DEBUG] devnum=" << devnum << " " << "fpbcount=" << fpbcount 
            << " fcount=" << fcount << " PCI buffer=" << std::hex << buffer;
    logwrite(function, message.str());
#endif

    server.frameinfo_mutex.lock();                 // protect access to frameinfo structure

    // Check to see if there is another frame with the same fpbcount...
    //
    // fpbcount is the frame counter that counts from 0 to FPB, so if there are 3 Frames Per Buffer
    // then for 10 frames, fcount goes from 0,1,2,3,4,5,6,7,8,9
    // and fpbcount goes from 0,1,2,0,1,2,0,1,2,0
    //
    try {
      if ( server.controller.at(devnum).frameinfo.count( fpbcount ) == 0 ) {  // ...no
        server.controller.at(devnum).frameinfo[ fpbcount ].tid      = fpbcount;
        server.controller.at(devnum).frameinfo[ fpbcount ].buf      = buffer;
        // If useframes is false then set framenum=0 because it doesn't mean anything,
        // otherwise set it to the fcount received from the API.
        //
        server.controller.at(devnum).frameinfo[ fpbcount ].framenum = server.useframes ? fcount : 0;
      }
      else {                                                                  // ...yes
        logwrite(function, "ERROR: frame buffer overrun! Try allocating a larger buffer");
        server.frameinfo_mutex.unlock();
        return;
      }
    }
    catch (std::out_of_range &) {
      message.str(""); message << "ERROR indexing controller devnum=" << devnum << " or frame at fpb=" << fpbcount;
      logwrite(function, message.str());
      server.frameinfo_mutex.unlock();
      return;
    }

    server.frameinfo_mutex.unlock();               // release access to frameinfo structure

#ifdef LOGLEVEL_DEBUG
    if ( server.useframes ) {
      logwrite(function, "*** [DEBUG] waiting for frame");
    }
    else {
      logwrite(function, "*** [DEBUG] firmware doesn't support frames");
    }
#endif

    // Write the frames in numerical order.
    // Don't let one thread get ahead of another when it comes to writing.
    //
    double start_time = get_clock_time();
    do {
      int this_frame = fcount;                     // the current frame
      int last_frame = server.camera.at(devnum).get_framecount();    // the last frame that has been written by this device
      int next_frame = last_frame + 1;             // the next frame in line
      if (this_frame != next_frame) {              // if the current frame is NOT the next in line then keep waiting
        usleep(5);
        double time_now = get_clock_time();
        if (time_now - start_time >= 1.0) {        // update progress every 1 sec so the user knows what's going on
          start_time = time_now;
          logwrite(function, "waiting for frames");//TODO presumably update GUI or set a global that the CLI can access
#ifdef LOGLEVEL_DEBUG
          message.str(""); message << "*** [DEBUG] this_frame=" << this_frame << " next_frame=" << next_frame;
          logwrite(function, message.str());
#endif
        }
      }
      else {                                       // otherwise it's time to write this frame to disk
        break;                                     // note that frameinfo_mutex remains locked now until completion
      }

      if ( server.common.get_abortstate() ) break; // provide the user a way to get out

    } while ( server.useframes );                  // some firmware doesn't support frames so get out after one frame if it doesn't

    // If not aborted then write this frame
    //
    if ( ! server.common.get_abortstate() ) {
#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "*** [DEBUG] calling server.write_frame for devnum=" << devnum << " fpbcount=" << fpbcount;
    logwrite(function, message.str());
#endif
      server.write_frame( devnum, fpbcount );
    }
    else {
      logwrite(function, "aborted!");
    }

    // Done with the frame identified by "fpbcount".
    // Erase it from the STL map so it's not seen again.
    //
    try {
      server.frameinfo_mutex.lock();               // protect access to frameinfo structure
      server.controller.at(devnum).frameinfo.erase( fpbcount );
    }
    catch (std::out_of_range &) {
      message.str(""); message << "ERROR erasing frameinfo for fpb=" << fpbcount << " at controller devnum=" << devnum;
      logwrite(function, message.str());
    }

    server.frameinfo_mutex.unlock();

    server.remove_framethread();  // framethread_count is decremented because a thread has completed

    return;
  }
  /**************** AstroCam::Interface::handle_frame *************************/


  /**************** AstroCam::Interface::add_framethread **********************/
  /**
   * @fn     add_framethread
   * @brief  call on thread creation to increment framethreadcount
   * @param  none
   * @return none
   *
   */
  inline void Interface::add_framethread() {
    this->framethreadcount_mutex.lock();
    this->framethreadcount++;
    this->framethreadcount_mutex.unlock();
  }
  /**************** AstroCam::Interface::add_framethread **********************/


  /**************** AstroCam::Interface::remove_framethread *******************/
  /**
   * @fn     remove_framethread
   * @brief  call on thread destruction to decrement framethreadcount
   * @param  none
   * @return none
   *
   */
  inline void Interface::remove_framethread() {
    this->framethreadcount_mutex.lock();
    this->framethreadcount--;
    this->framethreadcount_mutex.unlock();
  }
  /**************** AstroCam::Interface::remove_framethread *******************/


  /**************** AstroCam::Interface::get_framethread **********************/
  /**
   * @fn     get_framethread
   * @brief  return the number of active threads spawned for handling frames
   * @param  none
   * @return int, number of threads
   *
   */
  inline int Interface::get_framethread_count() {
    int count;
    this->framethreadcount_mutex.lock();
    count = this->framethreadcount;
    this->framethreadcount_mutex.unlock();
    return( count );
  }
  /**************** AstroCam::Interface::get_framethread **********************/


  /**************** AstroCam::Interface::init_framethread_count ***************/
  /**
   * @fn     init_framethread_count
   * @brief  initialize framethreadcount = 0
   * @param  none
   * @return none
   *
   */
  inline void Interface::init_framethread_count() {
    this->framethreadcount_mutex.lock();
    this->framethreadcount = 0;
    this->framethreadcount_mutex.unlock();
  }
  /**************** AstroCam::Interface::init_framethread_count ***************/


  /** AstroCam::Interface::CameraFits::CameraFits *****************************/
  /**
   * @fn     CameraFits
   * @brief  class constructor
   * @param  none
   * @return none
   *
   */
  Interface::CameraFits::CameraFits() {
    this->workbuf = NULL;
    this->workbuf_size = 0;
    this->devnum = 0;
    this->framecount = 0;
  }
  /** AstroCam::Interface::CameraFits::CameraFits *****************************/


  /**************** AstroCam::Interface::CameraFits::open_file ****************/
  /**
   * @fn     open_file
   * @brief  wrapper to open the current fits file object
   * @param  none
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::CameraFits::open_file() {
#ifdef LOGLEVEL_DEBUG
    std::string function = "AstroCam::Interface::CameraFits::open_file";
    std::stringstream message;
    message << "*** [DEBUG] devnum=" << this->get_devnum() 
            << " fits_name=" << this->info.fits_name 
            << " this->pFits=" << std::hex << this->pFits;
    logwrite(function, message.str());
#endif
    long error = this->pFits->open_file(this->info);
    return( error );
  }
  /**************** AstroCam::Interface::CameraFits::open_file ****************/


  /**************** AstroCam::Interface::CameraFits::close_file ***************/
  /**
   * @fn     close_file
   * @brief  wrapper to close the current fits file object
   * @param  none
   * @return none
   *
   */
  void Interface::CameraFits::close_file() {
    std::string function = "AstroCam::Interface::CameraFits::close_file";
#ifdef LOGLEVEL_DEBUG
    std::stringstream message;
    message << "*** [DEBUG] devnum=" << this->get_devnum() 
            << " fits_name=" << this->info.fits_name 
            << " this->pFits=" << std::hex << this->pFits;
    logwrite(function, message.str());
#endif
    try {
      this->pFits->close_file();
    }
    catch(...) { logwrite(function, "unknown error closing FITS file(s)"); }
    return;
  }
  /**************** AstroCam::Interface::CameraFits::close_file ***************/


  /** AstroCam::Interface::CameraFits::init_framecount ************************/
  /**
   * @fn     init_framecount 
   * @brief  initialize this->framecount=0, protected by mutex
   * @param  none
   * @return none
   *
   */
  inline void Interface::CameraFits::init_framecount() {
    server.framecount_mutex.lock();
    this->framecount = 0;
    server.framecount_mutex.unlock();
  }
  /** AstroCam::Interface::CameraFits::init_framecount ************************/


  /** AstroCam::Interface::CameraFits::get_framecount *************************/
  /**
   * @fn     get_framecount 
   * @brief  returns this->framecount, protected by mutex
   * @param  none
   * @return int framecount
   *
   */
  inline int Interface::CameraFits::get_framecount() {
    int count;
    server.framecount_mutex.lock();
    count = this->framecount;
    server.framecount_mutex.unlock();
    return( count );
  }
  /** AstroCam::Interface::CameraFits::get_framecount *************************/


  /** AstroCam::Interface::CameraFits::increment_framecount *******************/
  /**
   * @fn     increment_framecount 
   * @brief  increments this->framecount, protected by mutex
   * @param  none
   * @return none
   *
   */
  inline void Interface::CameraFits::increment_framecount() {
    server.framecount_mutex.lock();
    this->framecount++;
    server.framecount_mutex.unlock();
  }
  /** AstroCam::Interface::CameraFits::increment_framecount *******************/


  /**************** AstroCam::Interface::deinterlace **************************/
  /**
   * @fn     deinterlace
   * @brief  spawns the deinterlacing threads
   * @param  args contains: module, channel, bias
   * @return none
   *
   * This function spawns threads to perform the actual deinterlacing in order
   * to get it done as quickly as possible.
   * 
   * Called by write_frame(), which is called by the handle_frame thread.
   *
   */
  template <class T>
  void Interface::CameraFits::deinterlace(T* imbuf) {
    std::string function = "AstroCam::Interface::deinterlace";
    std::stringstream message;

    int nthreads = std::thread::hardware_concurrency();

    T* workbuf = (T*)this->workbuf;

#ifdef LOGLEVEL_DEBUG
    message << "*** [DEBUG] devnum=" << this->devnum << " nthreads=" << nthreads << " imbuf=" << std::hex << imbuf;
    logwrite(function, message.str());
#endif

    // Instantiate a DeInterlace class object,
    // which is constructed with the pointers need for the image and working buffers.
    // This object contains the functions needed for the deinterlacing,
    // which will get called by the threads created here.
    //
    DeInterlace deinterlace(this->devnum, imbuf, workbuf);

    // spawn threads to handle each sub-section of the image
    //
    {                                       // begin local scope
    std::vector<std::thread> threads;       // create a local scope vector for the threads
    for ( int section = 1; section <= nthreads; section++ ) {
      std::thread thr( &AstroCam::Interface::CameraFits::dothread_deinterlace<T>, 
                   std::ref(deinterlace), 
                   section );
      threads.push_back(std::move(thr));    // push the thread into a vector
    }

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "*** [DEBUG] devnum " << this->devnum << " waiting for deinterlacing threads";
    logwrite(function, message.str());
#endif

    // By joining all the deinterlace threads to this thread (NOT to each other!) then
    // this function will not return until all of the deinterlace threads have finished.
    // This is necessary because the DeInterlace object cannot be allowed to go out of scope
    // before all threads are done using it.
    //
    try {
      for (std::thread & thr : threads) {   // loop through the vector of threads
        if ( thr.joinable() ) thr.join();   // if thread object is joinable then join to this function. (not to each other)
      }
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR joining threads: " << e.what();
      logwrite(function, message.str());
    }
    catch(...) { logwrite(function, "unknown error joining threads"); }

    threads.clear();                        // deconstruct the threads vector
    }

    message.str(""); message << "deinterlacing for dev " << this->devnum << " complete";
    logwrite(function, message.str());

    return;
  }
  /**************** AstroCam::Interface::deinterlace **************************/


  /**************** AstroCam::Interface::dothread_deinterlace *****************/
  /**
   * @fn     dothread_deinterlace
   * @brief  run in a thread to perform the actual deinterlacing
   * @param  T &imagebuf, reference to image buffer
   * @param  T &workbuf, reference to buffer to write deinterlaced image
   * @param  int section, the section of the buffer this thread is working on
   * @return none
   *
   */
  template <class T>
//void Interface::CameraFits::dothread_deinterlace(T &imagebuf, T &workbuf, XeInterlace<T> &deinterlace, int section) {
  void Interface::CameraFits::dothread_deinterlace(DeInterlace<T> &deinterlace, int section) {
    std::string function = "AstroCam::Interface::CameraFits::dothread_deinterlace";
    std::stringstream message;
#ifdef LOGLEVEL_DEBUG
//  message << "*** [DEBUG] imagebuf=" << std::hex << imagebuf << " workbuf=" << workbuf << " section=" << section;
    message << "*** [DEBUG] " << deinterlace.test();
    logwrite(function, message.str());
    usleep(2000000);
#endif
//server.dosomething(imagebuf, section);
    return;
  }
  /**************** AstroCam::Interface::dothread_deinterlace *****************/

template <class T>
void Interface::dosomething(T buf, int section) {
std::stringstream message;
message << "section=" << section << " buf=" << std::hex << buf;
logwrite("AstroCam::Interface::dosomething", message.str());
usleep(5000000);
}


  /**************** AstroCam::Interface::test *********************************/
  /**
   * @fn     test
   * @brief  test routines
   * @param  string args contains test name and arguments
   * @param  reference to retstring for any return values
   * @return ERROR or NO_ERROR
   *
   * This is the place to put various debugging and system testing tools.
   * It is placed here, rather than in common, to allow for controller-
   * specific tests. This means some common tests may need to be duplicated
   * for each controller.
   *
   * The server command is "test", the next parameter is the test name,
   * and any parameters needed for the particular test are extracted as
   * tokens from the args string passed in.
   *
   * The input args string is tokenized and tests are separated by a simple
   * series of if..else.. conditionals.
   *
   */
  long Interface::test(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::test";
    std::stringstream message;
    std::vector<std::string> tokens;
    long error;

    Tokenize(args, tokens, " ");

    if (tokens.size() < 1) {
      logwrite(function, "no test name provided");
      return ERROR;
    }

    std::string testname = tokens[0];                                // the first token is the test name

    // ----------------------------------------------------
    // fitsname
    // ----------------------------------------------------
    // Show what the fitsname will look like.
    // This is a "test" rather than a regular command so that it doesn't get mistaken
    // for returning a real, usable filename. When using fitsnaming=time, the filename
    // has to be generated at the moment the file is opened.
    //
    if (testname == "fitsname") {
      std::string msg;
      this->common.set_fitstime( get_system_time() );                // must set common.fitstime first
      error = this->common.get_fitsname( msg );                      // get the fitsname (by reference)
      this->common.message.enqueue( msg );                           // queue the fitsname
      logwrite( function, msg );                                     // log ths fitsname
    } // end if (testname == fitsname)

    // ----------------------------------------------------
    // async [message]
    // ----------------------------------------------------
    // queue an asynchronous message
    // The [message] param is optional. If not provided then "test" is queued.
    //
    else
    if (testname == "async") {
      if (tokens.size() > 1) {
        if (tokens.size() > 2) {
          logwrite(function, "NOTICE: received multiple strings -- only the first will be queued");
        }
        logwrite( function, tokens[1] );
        this->common.message.enqueue( tokens[1] );
      }
      else {                                // if no string passed then queue a simple test message
        logwrite(function, "test");
        this->common.message.enqueue("test");
      }
      error = NO_ERROR;
    } // end if (testname == async)

    // ----------------------------------------------------
    // bw <nseq>
    // ----------------------------------------------------
    // Bandwidth test
    // This tests the exposure sequence bandwidth by running a sequence
    // of exposures, including reading the frame buffer -- everything except
    // for the fits file writing.
    //
    else
    if (testname == "bw") {
      message.str(""); message << "ERROR: test " << testname << " not implemented";
      logwrite(function, message.str());
      error = ERROR;
    } // end if (testname==bw)

    // ----------------------------------------------------
    // invalid test name
    // ----------------------------------------------------
    //
    else {
      message.str(""); message << "ERROR: test " << testname << " unknown";;
      logwrite(function, message.str());
      error = ERROR;
    }

    return error;
  }
  /**************** AstroCam::Interface::test *********************************/


  /**************** AstroCam::Interface::CameraFits::write ********************/
  /**
   * @fn     write
   * @brief  wrapper to write a fits file
   * @param  buf_in, void pointer to buffer containing image
   * @return ERROR or NO_ERROR
   *
   * Since this is part of the CameraFits class and camera_info is a member
   * object, we already know everything from this->info, except which
   * buffer is ready, and that is passed in as a void pointer.
   *
   * That pointer is cast to the appropriate type here, based on this->info.datatype,
   * and then the FITS_file class write_image function is called with the correctly
   * typed buffer. write_image is a template class function so it just needs to
   * be called with a buffer of the appropriate type.
   *
   */
  template <class T>
  long Interface::CameraFits::write(T* buf) {
    std::string function = "AstroCam::Interface::CameraFits::write";
    std::stringstream message;
    long retval;

if ( server.common.get_abortstate() ) {
  logwrite(function, "* * * * * GOT ABORT * * * * * skipping write !");
  return( NO_ERROR );
}

#ifdef LOGLEVEL_DEBUG
    message << "*** [DEBUG] buf=" << std::hex << buf << " this->pFits=" << this->pFits;
    logwrite(function, message.str());
#endif

    // Cast the incoming void pointer and call pFits->write_image() 
    // based on the datatype.
    //
    switch (this->info.datatype) {
      case USHORT_IMG: {
        retval = this->pFits->write_image( (uint16_t *)buf, this->info);
        break;
      }
      case SHORT_IMG: {
        retval = this->pFits->write_image( (int16_t *)buf, this->info);
        break;
      }
      case FLOAT_IMG: {
        retval = this->pFits->write_image( (uint32_t *)buf, this->info);
        break;
      }
      default:
        message.str("");
        message << "ERROR: unknown datatype: " << this->info.datatype;
        logwrite(function, message.str());
        retval = ERROR;
        break;
    }
    return( retval );
  }
  /**************** AstroCam::Interface::CameraFits::write ********************/


  /**************** AstroCam::Interface::CameraFits::alloc_workbuf ************/
  /**
   * @fn     alloc_workbuf
   * @brief  allocate workspace memory for deinterlacing
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This function calls an overloaded template class version with 
   * a generic pointer cast to the correct type.
   *
   */
  long Interface::CameraFits::alloc_workbuf() {
    std::string function = "AstroCam::Interface::CameraFits::alloc_workbuf";
    std::stringstream message;
    long retval = NO_ERROR;
    void* ptr=NULL;

    switch (this->info.datatype) {
      case USHORT_IMG: {
        this->alloc_workbuf( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->alloc_workbuf( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->alloc_workbuf( (uint32_t *)ptr );
        break;
      }
      default:
        message.str("");
        message << "ERROR: unknown datatype: " << this->info.datatype;
        logwrite(function, message.str());
        retval = ERROR;
        break;
    }
    return( retval );
  }
  /**************** AstroCam::Interface::CameraFits::alloc_workbuf ************/


  /**************** AstroCam::Interface::CameraFits::alloc_workbuf ************/
  /**
   * @fn     alloc_workbuf
   * @brief  allocate workspace memory for deinterlacing
   * @param  buf, pointer to template type T
   * @return pointer to the allocated space
   *
   * The actual allocation occurs in here, based on the template class pointer type.
   *
   */
  template <class T>
  void* Interface::CameraFits::alloc_workbuf(T* buf) {
    std::string function = "AstroCam::Interface::CameraFits::alloc_workbuf";
    std::stringstream message;

    // Maybe the size of the existing buffer is already just right
    //
    if ( this->info.image_size == this->workbuf_size ) return( (void*)this->workbuf );

    // But if it's not, then free whatever space is allocated, ...
    //
    if ( this->workbuf != NULL ) this->free_workbuf(buf);

    // ...and then allocate new space.
    //
    this->workbuf = (T*) new T [ this->info.image_size ];
    this->workbuf_size = this->info.image_size;

    message << "allocated " << this->workbuf_size << " bytes for device " << this->devnum << " deinterlacing buffer " << std::hex << this->workbuf;
    logwrite(function, message.str());
    return( (void*)this->workbuf );

  }
  /**************** AstroCam::Interface::CameraFits::alloc_workbuf ************/


  /**************** AstroCam::Interface::CameraFits::free_workbuf *************/
  /**
   * @fn     free_workbuf
   * @brief  free (delete) memory allocated by alloc_workbuf
   * @param  buf, pointer to template type T
   * @return none
   *
   * Must pass a pointer of the correct type because delete doesn't work on void.
   *
   */
  template <class T>
  void Interface::CameraFits::free_workbuf(T* buf) {
    std::string function = "AstroCam::Interface::CameraFits::free_workbuf";
    std::stringstream message;
    if (this->workbuf != NULL) {
      delete [] (T*)this->workbuf;
      this->workbuf = NULL;
      this->workbuf_size = 0;
      message << "deleted old deinterlacing buffer " << std::hex << this->workbuf;
      logwrite(function, message.str());
    }
  }
  /**************** AstroCam::Interface::CameraFits::alloc_workbuf ************/

}
