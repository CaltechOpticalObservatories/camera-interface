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
    std::string function = "Arc64::Callback::FrameCallback";
    std::stringstream message;

    message << "spawning \"handle_frame\" thread: frame=" << dFPBCount
            << " FrameCount=" << dPCIFrameCount
            << " buffer=0x" << std::uppercase << std::hex << pBuffer;
    logwrite(function, message.str());
//  std::thread(&AstroCam::AstroCam::handle_frame, dFPBCount, dPCIFrameCount, pBuffer).detahc();
  }
  /** Arc64::Callback::FrameCallback ******************************************/


  /** Arc64::Interface::arc_native ********************************************/
  /**
   * @fn     native
   * @brief  send a 3-letter command to the Leach controller
   * @param  int cmd
   * @param  int arg1
   * @param  int arg2
   * @param  int arg3
   * @param  int arg4
   * @return 0 on success, 1 on error
   *
   */
  long Interface::arc_native(int cmd, int arg1, int arg2, int arg3, int arg4) {
    std::string function = "Arc64::Interface::arc_native";
    std::stringstream message;
    int lReply;

    if (this->num_controllers < 1) {
      logwrite(function, "error: no connected PCI devices");
      return(1);
    }

    // send the command here
    //
    try { // TODO for Thur: make lReply a vector and report only one reply if all the same, report diff if one different.
      for (int dev=0; dev < this->num_controllers; dev++) {
        lReply = controller[dev].Command( arc::TIM_ID, cmd, arg1, arg2, arg3, arg4 );
        message.str(""); message << "command: 0x" << std::uppercase << std::hex << cmd << " received: 0x" << lReply;
        logwrite(function, message.str());
      }
    }
    catch ( std::runtime_error &e ) {
      message.str(""); message << "error: " << e.what() << " sending command: 0x" << std::uppercase << std::hex << cmd;
      logwrite(function, message.str());
      return(1);
      }
    catch ( ... ) {
      message.str(""); message << "unknown error sending command: 0x" << std::uppercase << std::hex << cmd;
      logwrite(function, message.str());
      return(1);
      }

    return 0;  //TODO needs to be updated with combined lReply
  }
  /** Arc64::Interface::arc_native ********************************************/


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
