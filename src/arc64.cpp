#include "arc64.h"

namespace Arc64 {

  Interface::Interface() {
  }

  Interface::~Interface() {
  }


  /** Arc64::Callback::ExposeCallback *****************************************/
  /**
   * @fn     ExposeCallback
   * @brief  callback function for expose
   * @param  
   * @return none
   *
   */
  void Callback::ExposeCallback( float fElapsedTime ) {
  }
  /** Arc64::Callback::ExposeCallback *****************************************/


  /** Arc64::Callback::ReadCallback *******************************************/
  /**
   * @fn     ReadCallback
   * @brief  callback function for pixel readout
   * @param  int dPixelCount
   * @return none
   *
   */
  void Callback::ReadCallback( int dPixelCount ) {
    std::cerr << std::setw(8) << dPixelCount << '\r';
    }
  /** Arc64::Callback::ReadCallback *******************************************/


  /** Arc64::Callback::FrameCallback ******************************************/
  /**
   * @fn     FrameCallback
   * @brief  callback function for frame received
   * @param  
   * @return none
   *
   */
  void Callback::FrameCallback( int   dFPBCount,
                                int   dPCIFrameCount,
                                int   dRows,
                                int   dCols,
                                void* pBuffer ) {
  }
  /** Arc64::Callback::FrameCallback ******************************************/


  /** Arc64::Interface::native ************************************************/
  /**
   * @fn     native
   * @brief  send a 3-letter command to the Leach controller
   * @param  cmdstr, string containing command and arguments
   * @return 0 on success, 1 on error
   *
   */
  long Interface::native(std::string cmdstr) {
    std::string function = "Arc64::Interface::native";
    std::stringstream message;
    std::vector<std::string> tokens;

    int lReply;
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
      message.str(""); message << "error: poorly formatted command: " << tokens[0] << " (expected 3 letters but got " << tokens[0].length() << ")";
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

    // send the command here
    //
    try { // TODO for Thur: make lReply a vector and report only one reply if all the same, report diff if one different.
      for (int dev=0; dev<this->num_controllers; dev++) {
        lReply = controller[dev].Command( arc::TIM_ID, cmd, arg[0], arg[1], arg[2], arg[3] );
        message.str(""); message << "command: " << cmd << " received: " << lReply;
        logwrite(function, message.str());
      }
    }
    catch ( std::runtime_error &e ) {
      message.str(""); message << "error: " << e.what() << " sending command: " << cmd;
      logwrite(function, message.str());
      return(1);
      }
    catch ( ... ) {
      message.str(""); message << "unknown error sending command: " << cmd;
      logwrite(function, message.str());
      return(1);
      }

    return 0;
  }
  /** Arc64::Interface::native ************************************************/


  /** Arc64::Interface::interface *********************************************/
  /**
   * @fn     interface
   * @brief  return a string containing the type of hardware interface
   * @param  iface, reference to a string to contain the return value
   * @return 0
   *
   */
  long Interface::interface(std::string &iface) {
    std::string function = "Arc64::Interface::interface";
    iface = "ARC-64-PCI";
    logwrite(function, iface);
    return(0);
  }
  /** Arc64::Interface::interface *********************************************/


  /** Arc64::Interface::connect_controller ************************************/
  /**
   * @fn     connect_controller
   * @brief  detect and open PCI driver(s)
   * @param  none
   * @return 0 on success, 1 on error
   *
   */
  long Interface::connect_controller() {
    std::string function = "Arc64::Interface::connect_controller";
    std::stringstream message;
    int dev=0;

    try {
      logwrite( function, "getting device bindings" );
      arc::CController cController;
      this->num_controllers = cController.GetDeviceBindingCount();    // get number of ARC PCI devices
      message.str("");
      message << "detected " << this->num_controllers << " ARC-64 device" << (this->num_controllers == 1 ? "":"s");
      logwrite( function, message.str() );

      for (dev=0; dev<this->num_controllers; dev++) {
        arc::CController cc;                                 // Instantiate a CController object,
        controller.push_back(cc);                            // and put it in the vector of controller objects.

        Callback *cb = new Callback;                         // Create a pointer to a Callback object,
        callback.push_back(cb);                              // and put it in the vector of callback objects.
                                                             // This will get passed to the arc::Expose function.

        message.str(""); message << "opening PCI device " << dev;
        logwrite( function, message.str() );
        controller[dev].OpenDriver( dev );                   // open the device
      }
    }
    catch ( std::runtime_error &e ) {
      message.str(""); message << "error " << e.what() << " opening PCI driver " << dev;
      logwrite(function, message.str());
      return(1);
    }
    catch ( ... ) {
      message.str(""); message << "unknown error opening PCI driver " << dev;
      logwrite(function, message.str());
      return(1);
    }

    return(0);
  }
  /** Arc64::Interface::connect_controller ************************************/


  /** Arc64::Interface::disconnect_controller *********************************/
  /**
   * @fn     disconnect_controller
   * @brief  close the PCI devices and release memory
   * @param  none
   * @return 0 on success, 1 on error
   *
   */
  long Interface::disconnect_controller() {
    std::string function = "Arc64::Interface::disconnect_controller";
    std::stringstream message;

    if (this->num_controllers < 1) {
      logwrite(function, "error: no connected PCI devices");
      return(1);
    }

    try {
      for (int dev=0; dev<this->num_controllers; dev++) {
        controller[dev].CloseDriver();
        controller[dev].UnmapDriver();
      }
      logwrite(function, "PCI device closed");
    }
  catch ( std::runtime_error &e ) {
    message.str(""); message << "error " << e.what() << " closing PCI driver";
    logwrite(function, message.str());
    return(1);
    }
  catch ( ... ) {
    logwrite(function, "unknown error closing PCI driver");
    return(1);
    }
  return(0);
  }
  /** Arc64::Interface::disconnect_controller *********************************/


  /** Arc64::Interface::arc_expose ********************************************/
  /**
   * @fn     arc_expose
   * @brief  triggers an exposure by calling the Arc64 API Expose()
   * @param  none
   * @return 
   *
   */
  long Interface::arc_expose(int nframes, int expdelay, int rows, int cols) {
    std::string function = "Arc64::Interface::arc_expose";
    std::stringstream message;
    bool abort=false;
    bool config=false;

    if (this->num_controllers < 1) {
      logwrite(function, "error: no connected PCI devices");
      return(1);
    }

    try {
      for (int dev=0; dev<this->num_controllers; dev++) {
        this->controller[dev].Expose( nframes, expdelay, rows, cols, abort, config, this->callback[dev] );
      }
    }
    catch ( std::runtime_error &e ) {
      message.str(""); message << "error " << e.what() << " calling arc::Expose";
      logwrite(function, message.str());
      return(1);
      }
    catch ( ... ) {
      logwrite(function, "unknown error calling arc::Expose");
      return(1);
    }

    return 0;
  }
  /** Arc64::Interface::arc_expose ********************************************/


  /** Arc64::Interface::arc_load_firmware *************************************/
  /**
   * @fn     arc_load_firmware
   * @brief  load a file into the timing board
   * @param  string timlodfile
   * @return 
   *
   */
  long Interface::arc_load_firmware(std::string timlodfile) {
    std::string function = "Arc64::Interface::arc_load_firmware";
    std::stringstream message;

    if (this->num_controllers < 1) {
      logwrite(function, "error: no connected PCI devices");
      return(1);
    }

    try {
      for (int dev=0; dev < this->num_controllers; dev++) {
        this->controller[dev].LoadControllerFile(timlodfile.c_str());
      }
    }
    catch ( std::runtime_error &e ) {
      message.str(""); message << "error " << e.what() << " calling arc::LoadControllerFile";
      logwrite(function, message.str());
      return(1);
      }
    catch ( ... ) {
      logwrite(function, "unknown error calling arc::LoadControllerFile");
      return(1);
    }
    return(0);
  }
  /** Arc64::Interface::arc_load_firmware *************************************/

}
