#include "arc66.h"

namespace Arc66 {

  Interface::Interface() {
  }

  Interface::~Interface() {
  }

  /** Arc66::Interface::interface *********************************************/
  long Interface::interface(std::string &iface) {
    std::string function = "Arc66::Interface::interface";
    iface = "ARC-66-PCIe";
    logwrite(function, iface);
    return(0);
  }

  /** Arc66::Interface::interface *********************************************/


  /** Arc66::Interface::native ************************************************/
  long Interface::native(std::string cmd) {
    std::string function = "Arc66::Interface::native";
    std::stringstream message;
    return 0;
  }
  /** Arc66::Interface::native ************************************************/


  /** Arc66::Interface::connect_controller ************************************/
  long Interface::connect_controller() {
    std::string function = "Arc66::Interface::connect_controller";
    std::stringstream message;
    int status=ARC_STATUS_ERROR;
    const char** pDevList;

    ArcDevice_FindDevices( &status );
    if (status == ARC_STATUS_ERROR) {
      logwrite(function, "ERROR finding devices");
      return(status);
    }

    pDevList = ArcDevice_GetDeviceStringList( &status );
    if (status == ARC_STATUS_ERROR) {
      logwrite(function, "ERROR getting device string list");
      return(status);
    }

    for (int i=0; i<ArcDevice_DeviceCount(); i++) {
      message.str(""); message << "opening device " << i << ": " << pDevList[i];
      logwrite(function, message.str());
      ArcDevice_Open( i, &status );                // open driver only (no memory allocation)
      if (status == ARC_STATUS_ERROR) {
        message.str(""); message << "error opening device " << i << ": " << pDevList[i];
        logwrite(function, message.str());
      }
    }

    ArcDevice_FreeDeviceStringList();

    return(status);
  }
  /** Arc66::Interface::connect_controller ************************************/


  /** Arc66::Interface::disconnect_controller *********************************/
  long Interface::disconnect_controller() {
    std::string function = "Arc66::Interface::disconnect_controller";
    std::stringstream message;
    int status=ARC_STATUS_ERROR;
    ArcDevice_Close();
    ArcDevice_UnMapCommonBuffer(&status);
    logwrite(function, "device closed");
    return(status);
  }
  /** Arc66::Interface::disconnect_controller *********************************/


  /**************** Arc66::Interface::expose **********************************/
  /**
   * @fn     expose
   * @brief  triggers an exposure
   * @param  none
   * @return 
   *
   */
  long Interface::expose() {
    std::string function = "Arc66::Interface::expose";
    int  status = ARC_STATUS_ERROR;
    int  _rows = this->get_rows();
    int  _cols = this->get_cols();

    // initialize frame counter
    //
    this->frameinfo_mutex.lock();
    set_framecount(0);
    this->frameinfo_mutex.unlock();

    ArcDevice_Expose_N(nframes, expdelay, _rows, _cols, this->ReadCallback, this->FrameCallback, &status);

    if (status != ARC_STATUS_OK) {
      Logf("(%s) failed: %s\n", ArcDevice_GetLastError());
    }
    logwrite(function, "exposure started");

    return(status);
  }
  /**************** Arc66::Interface::expose **********************************/

}
