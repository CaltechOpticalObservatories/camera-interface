#include "arc64.h"

namespace Arc64 {

  Interface::Interface() {
  }

  Interface::~Interface() {
  }


  /** Arc64::Interface::native ************************************************/
  long Interface::native(std::string cmd) {
    std::string function = "Arc64::Interface::native";
    std::stringstream message;
    return 0;
  }
  /** Arc64::Interface::native ************************************************/


  /** Arc64::Interface::interface *********************************************/
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
      int devcount = cController.GetDeviceBindingCount();    // get number of ARC PCI devices
      message.str("");
      message << "detected " << devcount << " ARC-64 device" << (devcount > 1 ? "s":"");
      logwrite( function, message.str() );

      for (dev=0; dev<devcount; dev++) {
        arc::CController cc;                                 // Instantiate a CController object
        controller.push_back(cc);                            // and put it in the vector of controller objects.

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
  long Interface::disconnect_controller() {
    std::string function = "Arc64::Interface::disconnect_controller";
    std::stringstream message;
    try {
      this->cController.CloseDriver();
      logwrite(function, "PCI device closed");
      this->cController.UnmapDriver();
      logwrite(function, "unmapped image buffer");
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


  /** Arc64::Interface::expose ************************************************/
  /**
   * @fn     expose
   * @brief  triggers an exposure
   * @param  none
   * @return 
   *
   */
  long Interface::expose() {
    std::string function = "Arc64::Interface::expose";
    logwrite(function, "not yet operational");
    return 0;
  }
  /** Arc64::Interface::expose ************************************************/


  /** Arc64::Callback::ExposeCallback ******************************************/
  /**
   * @fn     ExposeCallback
   * @brief  callback function for expose
   * @param  
   * @return none
   *
   */
  void Callback::ExposeCallback( float fElapsedTime ) {
  }
  /** Arc64::Callback::ExposeCallback ******************************************/


  /** Arc64::Callback::ReadCallback ********************************************/
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
  /** Arc64::Callback::ReadCallback ********************************************/


  /** Arc64::Callback::FrameCallback *******************************************/
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
  /** Arc64::Callback::FrameCallback *******************************************/

}
