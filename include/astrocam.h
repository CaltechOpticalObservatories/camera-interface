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
#include "fits.h"
#include "logentry.h"
#include "CArcBase.h"
#include "ArcDefs.h"
#include "CExpIFace.h"
#include "CConIFace.h"
#include "CArcPCI.h"
#include "CArcPCIe.h"
#include "CArcDevice.h"
#include "CArcBase.h"
#include "CooExpIFace.h"


namespace AstroCam {

  class Callback : public arc::gen3::CooExpIFace {  //!< Callback class inherited from the ARC API
    public:
      Callback(){}
      void exposeCallback( int devnum, std::uint32_t uiElapsedTime );
      void readCallback( int devnum, std::uint32_t uiPixelCount );
      void frameCallback( int devnum,
                          std::uint32_t uiFramesPerBuffer,
                          std::uint32_t uiFrameCount,
                          std::uint32_t uiRows,
                          std::uint32_t uiCols,
                          void* pBuffer );
  };

  template <typename T>
  class XeInterlace {
    private:
      T* imbuf;
      T* workbuf;

    public:
      XeInterlace(T* imbuf_in, T* workbuf_in) {
        this->imbuf   = imbuf_in;
        this->workbuf = workbuf_in;
      }

      void split_parallel();
      void split_serial();
      void quad_ccd();
      std::string test(T* buf) { 
        std::stringstream ret;
        ret << " buf=" << buf << " workbuf=" << std::hex << this->workbuf << " imbuf=" << this->imbuf;
        return (ret.str());
      };
  };


  /**************** AstroCam::DeInterlace *************************************/
  /**
   * This is the DeInterlace class.
   *
   * The data it contains are pointers to the PCI image buffer and
   * the working buffer where the deinterlacing is to take place.
   *
   * The functions it contains are the procedures for performing the deinterlacing.
   *
   * This is a template class so that the buffers are created of the proper type.
   *
   */
  template <typename T>
  class DeInterlace {
    private:
      T* imbuf;
      T* workbuf;
      int devnum;

    public:
      void split_parallel();
      void split_serial();
      void quad_ccd();

      DeInterlace(T* buf1, T* buf2) {
        this->imbuf = buf1;
        this->workbuf = buf2;
      }

      DeInterlace(int devnum, T* buf1, T* buf2) {
        this->devnum = devnum;
        this->imbuf = buf1;
        this->workbuf = buf2;
      }

      std::string test() {
        std::stringstream ret;
        ret << "imbuf=" << std::hex << imbuf << " workbuf=" << std::hex << workbuf << " this->devnum=" << std::dec << this->devnum;
        return ( ret.str() );
      }
  };
  /**************** AstroCam::DeInterlace *************************************/


  class Interface {
    private:
      int rows;
      int cols;
      int bufsize;
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
//    std::string deinterlace;     //!< deinterlacing
      unsigned short* pResetbuf;   //!< buffer to hold reset frame (first frame of CDS pair)
      long* pCDSbuf;               //!< buffer to hold CDS subtracted image
      void* workbuf;               //!< workspace for performing deinterlacing
      int num_deinter_thr;         //!< number of threads that can de-interlace an image
      int numdev;                  //!< total number of Arc devices detected in system
      std::vector<int> devlist;    //!< vector of all opened and connected devices

      void retval_to_string( std::uint32_t check_retval, std::string& retstring );

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

      // The frameinfo structure holds frame information for each frame
      // received by the callback. This is used to keep track of all the 
      // threads spawned by handle_frame.
      //
      typedef struct {
        int   tid;                      //!< use fpbcount as the thread ID here
        int   framenum;                 //!< the current frame from ARC_API's fcount, counts from 1
        int   rows;                     //!< number of rows in this frame
        int   cols;                     //!< number of cols in this frame
        void* buf;                      //!< pointer to the start of memory holding this frame
        bool  inuse;                    //!< this thread ID is in use, set when thread is spawned, cleared when handle_frame is done
      } frameinfo_t;

      std::map<int, frameinfo_t> frameinfo;

      // The Controller Info structure hold information for a controller.
      // This will be put into a vector object so there will be one of these
      // for each PCI(e) device.
      //
      struct controller_info {
        arc::gen3::CArcDevice* pArcDev;  //!< arc::CController object pointer
        Callback* pCallback;             //!< Callback class object must be pointer because the API functions are virtual
        bool connected;                  //!< true if controller connected (requires successful TDL command)
        bool firmwareloaded;             //!< true if firmware is loaded, false otherwise
        int devnum;                      //!< this controller's devnum
        std::string devname;             //!< comes from arc::gen3::CArcPCI::getDeviceStringList()
        std::uint32_t retval;            //!< convenient place to hold return values for threaded commands to this controller
        std::map<int, frameinfo_t>  frameinfo;  //!< STL map of frameinfo structure (see above)
      };

      // Vector of controller structures.
      // Interface::connect_controller() will create an object for each device
      // found in the system and store a pointer to each object in this vector.
      //
      std::vector<controller_info> controller;

//    std::vector< XeInterlace<T> > deinter;

      std::mutex frameinfo_mutex;       //!< protects access to frameinfo
      std::mutex framecount_mutex;      //!< protects access to frame count

      std::atomic<int> framethreadcount;
      std::mutex framethreadcount_mutex;
      inline void add_framethread();
      inline void remove_framethread();
      inline int get_framethread_count();
      inline void init_framethread_count();

      // Ths CameraFits class is a sub-class of Interface and is here to contain
      // the Common::Information class and FITS_file class objects.
      // There will be a vector of CameraFits class objects which matches the
      // vector of controller objects.
      //
      class CameraFits {
        private:
          int devnum;
          int framecount;               //!< keep track of the number of frames received per expose
          void* workbuf;                //!< pointer to workspace for performing deinterlacing
          long workbuf_size;

        public:
          CameraFits();                 //!< class constructor
          ~CameraFits() { };            //!< no deconstructor
          Common::Information info;     //!< this is the main camera info object
          FITS_file *pFits;             //!< FITS container object has to be a pointer here

          // Functions
          //
          long alloc_workbuf();
          int get_devnum() { return this->devnum; }
          void set_devnum(int devnum) { this->devnum = devnum; }

          inline void init_framecount();
          inline int get_framecount();
          inline void increment_framecount();

          template <class T>
          void deinterlace(T* imbuf);

/*
          template <class T>
          static void dothread_deinterlace(T &imagebuf, T &workbuf, XeInterlace<T> &deinterlace, int section);
*/

          template <class T>
          static void dothread_deinterlace(DeInterlace<T> &tc, int section);

          template <class T>
          void* alloc_workbuf(T* buf);

          template <class T>    
          void free_workbuf(T* buf);

          template <class T>
          long write(T* buf);           //!< wrapper for this->pFits->write_image()

          long open_file();             //!< wrapper for this->pFits->open_file()
          void close_file();            //!< wrapper for this->pFits->close_file()
      };

      // Vector of CameraFits objects, created by Interface::connect_controller()
      //
      std::vector<CameraFits> camera;

//    std::vector<arc::gen3::CArcDevice*> controller; //!< vector of arc::CController object pointers, one for each PCI device

      // This is used for Archon. May or may not use modes for Astrocam, TBD.
      //
      bool modeselected;                //!< true if a valid mode has been selected, false otherwise

      bool useframes;                   //!< Not all firmware supports frames.

      FITS_file fits_file;              //!< instantiate a FITS container object

template <class T>
void dosomething(T buf, int section);
      // Functions
      //
      long test(std::string args, std::string &retstring);
      long interface(std::string &iface);
      long connect_controller(std::string devices_in);
      long disconnect_controller();
      long configure_controller();
      long get_parameter(std::string parameter, std::string &retstring);
      long set_parameter(std::string parameter);
      long access_useframes(std::string& useframes);
      long access_nframes(std::string valstring);
      int get_driversize();
      long load_firmware(std::string &retstring);
      long load_firmware(std::string timlodfile, std::string &retstring);
      long set_camera_mode(std::string mode);
      long exptime(std::string exptime_in, std::string &retstring);
      long geometry(std::string args, std::string &retstring);
      long bias(std::string args, std::string &retstring);
      long buffer(std::string size_in, std::string &retstring);

      void set_imagesize(int rowsin, int colsin, int* status);

      long expose(std::string nseq_in);
      long native(std::string cmdstr);
      long native(std::string cmdstr, std::string &retstring);
      long write_frame(int devnum, int fpbcount);
      static void dothread_load( controller_info &controller, std::string timlodfile );
      static void dothread_expose( controller_info &controller, CameraFits &camera );
      static void dothread_native( controller_info &controller, std::vector<uint32_t> cmd );
      static void handle_frame( int devnum, uint32_t fpbcount, uint32_t fcount, void* buffer );

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
      int set_rows(int r) { if (r>0) this->rows = r; return r; };
      int set_cols(int c) { if (c>0) this->cols = c; return c; };
      int get_image_rows() { return this->rows; };
      int get_image_cols() { return this->cols; };

  };

}

#endif
