/**
 * @file    astrocam.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * This is where you put things that are common to both ARC interfaces,
 * ARC-64 PCI and ARC-66 PCIe.
 *
 */
#ifndef ASTROCAM_H
#define ASTROCAM_H

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>
#include <functional>              //!< pass by reference to threads
#include <thread>
#include <iostream>
#include <vector>
#include <initializer_list>

#include "utilities.h"
#include "common.h"
#include "config.h"
#include "logentry.h"
#include "CArcBase.h"
#include "ArcDefs.h"
#include "CExpIFace.h"
#include "CConIFace.h"
#include "CArcPCI.h"
#include "CArcPCIe.h"
#include "CArcDevice.h"
#include "CArcBase.h"
#include "CExpIFace.h"


namespace AstroCam {

  class Callback : public arc::gen3::CExpIFace, arc::gen3::CConIFace {  //!< Callback class inherited from the ARC API
    public:
      Callback(){}
      void exposeCallback( float fElapsedTime );
      void readCallback( std::uint32_t uiPixelCount );
      void frameCallback( std::uint32_t uiFramesPerBuffer,
                          std::uint32_t uiFrameCount,
                          std::uint32_t uiRows,
                          std::uint32_t uiCols,
                          void* pBuffer );
  };

  class Interface {
    private:
      int rows;
      int cols;
      int bufsize;
      int framecount;
      bool isabort_exposure;
      int FITS_STRING_KEY;
      int FITS_DOUBLE_KEY;
      int FITS_INTEGER_KEY;
      int FITS_BPP16;
      int FITS_BPP32;

      int nfilmstrip;              //!< number of filmstrip frames (for enhanced-clocking dual-exposure mode)
      int deltarows;               //!< number of delta rows (for enhanced-clocking dual-exposure mode)
      int nfpseq;                  //!< number of frames per sequence
      int nframes;                 //!< total number of frames to acquire from controller, per "expose"
      int nsequences;              //!< number of sequences
      int expdelay;                //!< exposure delay
      int imnumber;                //!< 
      int nchans;                  //!<
      int writefreq;               //!<
      bool iscds;                  //!< is CDS mode enabled?
      bool iscdsneg;               //!< is CDS subtraction polarity negative? (IE. reset-read ?)
      bool isutr;                  //!< is SUTR mode enabled?
      std::string basename;        //!<
      std::string imdir;
      std::string fitsname;
      std::vector<int> validchans; //!< outputs supported by detector / controller
      std::string deinterlace;     //!< deinterlacing
      unsigned short* pResetbuf;   //!< buffer to hold reset frame (first frame of CDS pair)
      long* pCDSbuf;               //!< buffer to hold CDS subtracted image
      void* workbuf;               //!< workspace for performing deinterlacing
      unsigned long workbuf_sz;
      int num_deinter_thr;         //!< number of threads that can de-interlace an image
      int numdev;                  //!< total number of Arc devices detected in system
      std::vector<int> devlist;    //!< vector of all opened and connected devices

    public:
      Interface();
      ~Interface();

      // Class Objects
      //
      Config config;
      Common::Common common;            //!< instantiate a Common object
      Common::Information camera_info;  //!< this is the main camera_info object
      Common::Information fits_info;    //!< used to copy the camera_info object to preserve info for FITS writing
      Common::FitsKeys userkeys;        //!< create a FitsKeys object for FITS keys specified by the user

      // The Controller Info structure hold information for a controller.
      // This will be put into a vector object so there will be one of these
      // for each PCI(e) device.
      //
      struct controller_info {
        arc::gen3::CArcDevice* pArcDev;  //!< arc::CController object pointer
        Callback* callback;              //!< Callback class object must be pointer because the API functions are virtual
        bool connected;                  //!< true if controller connected (requires successful TDL command)
        bool firmwareloaded;             //!< true if firmware is loaded, false otherwise
        int devnum;                      //!< this controller's devnum
        std::string devname;             //!< comes from arc::gen3::CArcPCI::getDeviceStringList()
        std::uint32_t retval;            //!< convenient place to hold return values for threaded commands to this controller
      };

      std::vector<controller_info> controller;

//    std::vector<arc::gen3::CArcDevice*> controller; //!< vector of arc::CController object pointers, one for each PCI device

      // This is used for Archon. May or may not use modes for Astrocam, TBD.
      //
      bool modeselected;                //!< true if a valid mode has been selected, false otherwise

      // Functions
      //
      long interface(std::string &iface);
      long connect_controller(std::string devices_in);
      long disconnect_controller();
      long configure_controller();
      long get_parameter(std::string parameter, std::string &retstring);
      long set_parameter(std::string parameter);
      long access_nframes(std::string valstring);
      int get_driversize();
      long write_frame();
      long load_firmware();
      long load_firmware(std::string timlodfile);
      long set_camera_mode(std::string mode);
      long exptime(std::string exptime_in, std::string &retstring);
      long geometry(std::string args, std::string &retstring);
      long bias(std::string args, std::string &retstring);

      // Functions fully defined here (no code in astrocam.c)
      //
      int keytype_string()  { return this->FITS_STRING_KEY;  };
      int keytype_double()  { return this->FITS_DOUBLE_KEY;  };
      int keytype_integer() { return this->FITS_INTEGER_KEY; };
      int fits_bpp16()      { return this->FITS_BPP16;       };
      int fits_bpp32()      { return this->FITS_BPP32;       };
      int get_rows() { return this->rows; };
      int get_cols() { return this->cols; };
      int get_bufsize() { return this->bufsize; };
      bool get_abortexposure() { return this->isabort_exposure; };
      void abort_exposure() { this->isabort_exposure = true; };
      int set_rows(int r) { if (r>0) this->rows = r; return r; };
      int set_cols(int c) { if (c>0) this->cols = c; return c; };
      int get_image_rows() { return this->rows; };
      int get_image_cols() { return this->cols; };
      void set_framecount( int num ) { this->framecount = num; };
      void increment_framecount() { this->framecount++; };
      int get_framecount() { return this->framecount; };

      void set_imagesize(int rowsin, int colsin, int* status);

      long expose(std::string nseq_in);
      long native(std::string cmdstr);
      static void dothread_load( controller_info &controller, std::string timlodfile);
      static void dothread_native( controller_info &controller, std::vector<uint32_t> cmd);
      static void handle_frame( uint32_t uiFrameCount, void* pBuffer );
      long buffer(std::string size_in);
  };

}

#endif
