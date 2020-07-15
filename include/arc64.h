#ifndef ARC64_H
#define ARC64_H

#include "common.h"
#include "config.h"
#include "logentry.h"
#include "utilities.h"
#include "Defs.h"
#include "CController.h"  // defines namesspace "arc"
#include "CExpIFace.h"    // defines arc::Expose callbacks

namespace Arc64 {

  class Callback : public arc::CExpIFace {
    public:
      Callback(){}
      void ExposeCallback( float fElapsedTime );
      void ReadCallback( int dPixelCount );
      void FrameCallback( int   dFPBCount,
                          int   dPCIFrameCount,
                          int   dRows,
                          int   dCols,
                          void* pBuffer );
  };

  class Interface {
    private:
    public:
      Interface();
      ~Interface();

      Common::Common    common;              //!< instantiate a Common object

      std::vector<arc::CController> controller;  //!< vector of arc::CController objects, one for each PCI device
      std::vector<Callback*> callback;           //!< vector of Callback interface object pointers

      int num_controllers;                   //!< number of installed controllers (detected PCI drivers)

//    arc::CController  cController;
//    arc::CController *pController;

      long interface(std::string &iface);
      long connect_controller();
      long disconnect_controller();
      long arc_native(int cmd, int arg1, int arg2, int arg3, int arg4);
      long arc_expose(int nframes, int expdelay, int rows, int cols);
      long arc_load_firmware(std::string timlodfile);
  };

}

#endif
