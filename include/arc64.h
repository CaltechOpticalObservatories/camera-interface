#ifndef ARC64_H
#define ARC64_H

#include "common.h"
#include "config.h"
#include "logentry.h"
#include "utilities.h"
#include "CController.h"  // defines namesspace "arc"
#include "astrocam.h"     // defines namespace "AstroCam"

namespace Arc64 {

  class Interface : public AstroCam::Information, public AstroCam::CommonInterface {

    private:

    public:
      Interface();
      ~Interface();

      Common::Common    common;              //!< instantiate a Common object

      CommonInterface astrocam;              //!< common astrocam interface

      Information camera_info;               //!< this is the main camera_info object
      Information fits_info;                 //!< used to copy the camera_info object to preserve info for FITS writing

      std::vector<arc::CController> controller;
      arc::CController  cController;
      arc::CController *pController;

      long interface(std::string &iface);
      long connect_controller();
      long disconnect_controller();
      long native(std::string cmd);
      long expose();
  };

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

}

#endif
