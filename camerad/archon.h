/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ARCHON_H
#define ARCHON_H

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>
#include <fenv.h>
#include <memory>                  // for std::unique and friends (c++17)
#include <Python.h>

#include "pyhelper.h"
#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "fits.h"
#include "opencv2/opencv.hpp"

#define MAXADCCHANS 16             //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
#define MAXADMCHANS 72             //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
#define BLOCK_LEN 1024             //!< Archon block size
#define REPLY_LEN 100 * BLOCK_LEN  //!< Reply buffer size (over-estimate)

// Archon commands
//
#define  SYSTEM        std::string("SYSTEM")
#define  STATUS        std::string("STATUS")
#define  FRAME         std::string("FRAME")
#define  CLEARCONFIG   std::string("CLEARCONFIG")
#define  POLLOFF       std::string("POLLOFF")
#define  POLLON        std::string("POLLON")
#define  APPLYALL      std::string("APPLYALL")
#define  POWERON       std::string("POWERON")
#define  POWEROFF      std::string("POWEROFF")
#define  APPLYCDS      std::string("APPLYCDS")
#define  APPLYSYSTEM   std::string("APPLYSYSTEM")
#define  RESETTIMING   std::string("RESETTIMING")
#define  LOADTIMING    std::string("LOADTIMING")
#define  HOLDTIMING    std::string("HOLDTIMING")
#define  RELEASETIMING std::string("RELEASETIMING")
#define  LOADPARAMS    std::string("LOADPARAMS")
#define  TIMER         std::string("TIMER")
#define  FETCHLOG      std::string("FETCHLOG")
#define  UNLOCK        std::string("LOCK0")

// Minimum required backplane revisions for certain features
//
#define REV_RAMP           std::string("1.0.548")
#define REV_SENSORCURRENT  std::string("1.0.758")
#define REV_HEATERTARGET   std::string("1.0.1087")
#define REV_FRACTIONALPID  std::string("1.0.1054")
#define REV_VCPU           std::string("1.0.784")

namespace Archon {

  // Archon hardware-based constants.
  // These shouldn't change unless there is a significant hardware change.
  //
  const int nbufs = 3;             //!< total number of frame buffers  //TODO rename to maxnbufs?
  const int nmods = 12;            //!< number of modules per controller
  const int nadchan = 4;           //!< number of channels per ADC module

  // Parameter defaults, unless overridden by the configuration file
  //
  const int DEF_TRIGIN_EXPOSE_ENABLE   = 1;
  const int DEF_TRIGIN_EXPOSE_DISABLE  = 0;
  const int DEF_TRIGIN_UNTIMED_ENABLE  = 1;
  const int DEF_TRIGIN_UNTIMED_DISABLE = 0;
  const int DEF_TRIGIN_READOUT_ENABLE  = 1;
  const int DEF_TRIGIN_READOUT_DISABLE = 0;
  const int DEF_SHUTENABLE_ENABLE      = 1;
  const int DEF_SHUTENABLE_DISABLE     = 0;

  // ENUM list for each readout type
  //
  enum ReadoutType {
    NONE,
    NIRC2,
    NIRC2VIDEO,
    TEST,
    NUM_READOUT_TYPES
    };


  const int IMAGE_RING_BUFFER_SIZE = 5;

  const int CDS_OFFS = 100;  /// offset to add to read frame for cds images before subtraction

  /***** Archon::PythonProc ***************************************************/
  /**
   * @class  PythonProc
   * @brief  This is the class for external Python processors
   *
   */
  class PythonProc {
    private:
    public:
      PythonProc();
      ~PythonProc();

      CPyInstance hInstance;
      CPyObject pName;
      CPyObject pModule;

  };
  /***** Archon::PythonProc ***************************************************/


  /***** Archon::DeInterlace **************************************************/
  /**
   * @class  DeInterlace
   * @brief  This is the deinterlacing class
   *
   */
  template <typename T>
  class DeInterlace {
    public:
      cv::Mat resetframe;
      cv::Mat readframe;
    private:
      T* cdsbuf;
      T* workbuf;
      T* imbuf;
      bool iscds;
      long bufsize;
      int cols;
      int rows;
      int readout_type;
      long frame_rows;
      long frame_cols;
      long cubedepth;


      /***** Archon::DeInterlace::test ****************************************/
      /**
       * @brief     test
       * @param[in] bufrows
       * @details 
       *
       */
      void test( int bufrows ) {

        for (long pix=0; pix < 256*256; pix++) {
          this->imbuf[pix] = pix % 65535;
        }

        cv::Mat image;

        image = cv::Mat( 256, 256, CV_16U, this->imbuf );

        return;
      }
      /***** Archon::DeInterlace::test ****************************************/


      /***** Archon::DeInterlace::nirc2_video *********************************/
      /**
       * @brief      
       * @param[in]  bufrows
       * @details 
       *
       */
      void nirc2_video( int bufrows ) {
#ifdef LOGLEVEL_DEBUG
        std::stringstream message;
        message.str(""); message << "[DEBUG] bufrows=" << bufrows << " this->rows=" << this->rows 
                                 << " this->cols=" << this->cols;
        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif

        cv::Mat raw    = cv::Mat( bufrows,   this->cols, CV_16U, this->imbuf   );
        cv::Mat signal = cv::Mat( bufrows/2, this->cols, CV_16U, cv::Scalar(0) );
        cv::Mat reset  = cv::Mat( bufrows/2, this->cols, CV_16U, cv::Scalar(0) );

        cv::Mat deinter_reset  = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );
        cv::Mat deinter_signal = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );

        // Copy pairs of rows from the raw to the reset and signal frames
        //
        for ( int rawrow=0, newrow=0; rawrow < bufrows-4; rawrow+=4, newrow+=2 ) {
          raw.row( rawrow+0 ).copyTo(  reset.row(newrow+0) );
          raw.row( rawrow+1 ).copyTo(  reset.row(newrow+1) );
          raw.row( rawrow+2 ).copyTo( signal.row(newrow+0) );
          raw.row( rawrow+3 ).copyTo( signal.row(newrow+1) );
        }

        int workindex=0;
        this->nirc2( workindex, reset, deinter_reset );
        this->nirc2( workindex, signal, deinter_signal );

/***
        // Display live video of subtracted image
        //
        cv::Mat diff = deinter_reset - deinter_signal;
        cv::imshow( "image", diff );
        cv::waitKey(1);

        std::stringstream message;
        for ( int row=0; row<10; row++ ) {
          message.str(""); message << "reset:" << reset.at<uint16_t>(row,0) << " signal:" << signal.at<uint16_t>(row,0) << " diff:" << diff.at<uint16_t>(row,0);
          logwrite( "Archon::DeInterlace::nirc2_video", message.str() );
        }
***/

        return;
      }
      /***** Archon::DeInterlace::nirc2_video *********************************/


      /***** Archon::DeInterlace::nirc2 ***************************************/
      /**
       * @brief      NIRC2 deinterlacing
       * @param[in]  bufrows  rows in raw buffer, which is detector_pixels[1] * depth
       * @details 
       *
       * ~~~
       *                                               Q1 +---------+---------+ Q2
       * +---------+---------+---------+---------+        | <------ | ------> |
       * |         |         |         |         |        |       ^ | ^       |
       * |   Q1    |   Q2    |   Q3    |   Q4    | ===>   +---------+---------+
       * |         |         |         |         |        |       v | v       |
       * +---------+---------+---------+---------+        | <------ | ------> |
       * :         :         :         :         :     Q3 +---------+---------+ Q4
       * :         :         :         :         :
       * (for multiple frames, this raw buf is   
       *  extended in this dimension for each frame)
       *                                         
       * ~~~
       *
       */
      void nirc2( int bufrows ) {
#ifdef LOGLEVEL_DEBUG
        std::stringstream message;
        message.str(""); message << "[DEBUG] bufrows=" << bufrows << " this->rows=" << this->rows 
                                 << " this->cols=" << this->cols
                                 << " this->frame_rows=" << this->frame_rows << " this->frame_cols=" << this->frame_cols;
        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif
        // Create openCV image to hold entire imbuf (all cubes)
        //
        cv::Mat image = cv::Mat( bufrows, this->cols, CV_16U, this->imbuf );

        // Create an empty openCV image for performing the deinterlacing work.
        // Note that this "work" Mat image is a single frame!
        // If this->imbuf is a datacube then "work" will only hold the last frame by the time
        // it gets back here. So if you wait until this->nirc2() returns, this work image
        // may not be what you want. Consider it a temporary workspace only.
        //
        cv::Mat work  = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );

        int workindex=0;
        this->nirc2( workindex, image, work );

        return;
      }
      /***** Archon::DeInterlace::nirc2 ***************************************/


      /***** Archon::DeInterlace::nirc2 ***************************************/
      /**
       * @brief      NIRC2 deinterlacing
       * @param[in]  workindex  reference to index counter (where in deinterlacing)
       * @param[in]  image      reference to cv::Mat array of raw image
       * @param[out] work       reference to cv::Mat array for performing deinterlacing
       * @details 
       *
       * This function is overloaded.
       * This format is used for processing NIRC2 video. The deinterlacing is the
       * same but it operates on a cv::Mat image and returns a deinterlaced cv::Mat image.
       *
       * Note that the "work" Mat image for deinterlacing is a single frame!
       * If the image buffer is a datacube then "work" will only hold the last frame.
       * So if you want to save a deinterlaced image then you might need to get it
       * before this function finishes. Consider "work" a temporary workspace only.
       *
       * ~~~
       *                                               Q1 +---------+---------+ Q2
       * +---------+---------+---------+---------+        | <------ | ------> |
       * |         |         |         |         |        |       ^ | ^       |
       * |   Q1    |   Q2    |   Q3    |   Q4    | ===>   +---------+---------+
       * |         |         |         |         |        |       v | v       |
       * +---------+---------+---------+---------+        | <------ | ------> |
       * :         :         :         :         :     Q3 +---------+---------+ Q4
       * :         :         :         :         :
       * (for multiple frames, this raw buf is   
       *  extended in this dimension for each frame)
       * ~~~
       *
       */
      void nirc2( int &workindex, cv::Mat &image, cv::Mat &work ) {
#ifdef LOGLEVEL_DEBUG
        std::stringstream message;
        message.str(""); message << "[DEBUG] this->rows=" << this->rows << " this->cols=" << this->cols;
        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif

        int taps=8;

        int quad_rows  = this->frame_rows / 2;
        int quad_cols  = this->frame_cols / 2;

        // Define images for each quadrant
        //
        cv::Mat Q1, Q2, Q3, Q4;

        // Define images for the de-interlaced quadrants
        //
        cv::Mat Q1d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );
        cv::Mat Q2d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );
        cv::Mat Q3d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );
        cv::Mat Q4d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );

        // Define images for the quadrants that need to be flipped
        //
        cv::Mat Q2f, Q3f, Q4f;

        // Loop through each cube,
        // cutting each from the main image buffer, deinterlacing and
        // performing needed flips before copying into the buffer that is passed to the FITS writer.
        //
        for ( int i=0, j=1; i < this->cubedepth; i++, j++ ) {
          // Cut the quadrants out of the image
          //
          Q1 = image( cv::Range( i * quad_rows, j * quad_rows), cv::Range( 0 * quad_cols, 1 * quad_cols ) );
          Q2 = image( cv::Range( i * quad_rows, j * quad_rows), cv::Range( 1 * quad_cols, 2 * quad_cols ) );
          Q3 = image( cv::Range( i * quad_rows, j * quad_rows), cv::Range( 2 * quad_cols, 3 * quad_cols ) );
          Q4 = image( cv::Range( i * quad_rows, j * quad_rows), cv::Range( 3 * quad_cols, 4 * quad_cols ) );

          // Perform the deinterlacing here
          //
          for ( int row=0, idx=0; row < quad_rows; row++ ) {
            for ( int tap=0; tap < taps; tap++ ) {
              for ( int col=0; col < quad_cols; col+=8 ) {
                int loc = row*quad_cols + col + tap;
                int r   = (int) (loc/quad_rows);
                int c   = (int) (loc%quad_cols);
                int R   = (int) (idx/quad_rows);
                int C   = (int) (idx%quad_cols);
                idx++;
                Q1d.at<uint16_t>(r,c) = Q1.at<uint16_t>(R,C);
                Q2d.at<uint16_t>(r,c) = Q2.at<uint16_t>(R,C);
                Q3d.at<uint16_t>(r,c) = Q3.at<uint16_t>(R,C);
                Q4d.at<uint16_t>(r,c) = Q4.at<uint16_t>(R,C);
              }
            }
          }

          // Perform the quadrant flips here
          //
          cv::flip( Q2d, Q2f,  1 );  // flip horizontally
          cv::flip( Q3d, Q3f,  0 );  // flip vertically
          cv::flip( Q4d, Q4f, -1 );  // flip horizontally and vertically

          // Copy the modified quadrant images into a work image
          //
          for ( int row=0, R0=0, R1=quad_rows; row<quad_rows; row++, R0++, R1++ ) {
            for ( int col=0, C0=0, C1=quad_cols; col<quad_cols; col++, C0++, C1++ ) {
              work.at<uint16_t>(R1,C0) = (uint16_t)Q1d.at<uint16_t>(row,col);
              work.at<uint16_t>(R1,C1) = (uint16_t)Q2f.at<uint16_t>(row,col);
              work.at<uint16_t>(R0,C1) = (uint16_t)Q4f.at<uint16_t>(row,col);
              work.at<uint16_t>(R0,C0) = (uint16_t)Q3f.at<uint16_t>(row,col);
            }
          }

          // Subtract the image from 65535 because for NIRC2 the counts decrease
          // with increasing signal.
          //
          cv::subtract( 65535, work, work );

          // Copy assembled image into the FITS buffer, this->workbuf
          //
          for ( int row=0; row<this->frame_rows; row++ ) {
            for ( int col=0; col<this->frame_cols; col++ ) {
//            *( this->workbuf + workindex++ ) = 65535 - (uint16_t)(work.at<uint16_t>(row,col) - 0);
              *( this->workbuf + workindex++ ) = (uint16_t)(work.at<uint16_t>(row,col));
            }
          }
          if ( this->iscds && i==0 ) work.copyTo( this->resetframe );  // this is the reset frame
          if ( this->iscds && i==1 ) work.copyTo( this->readframe  );  // this is the read frame

        } // end of loop over cubes

        if ( !this->iscds ) return;

        // Add CDS_OFFS to the read frame to ensure that it is always greater than the reset frame
        //
        cv::add( CDS_OFFS, this->readframe, this->readframe );

        // Copy assembled image into the FITS buffer, this->cdsbuf
        //
        cv::Mat diff = this->readframe - this->resetframe;
        unsigned long index=0;
        for ( int row=0; row<this->frame_rows; row++ ) {
          for ( int col=0; col<this->frame_cols; col++ ) {
            *( this->cdsbuf + index++ ) = (uint16_t)(diff.at<uint16_t>(row,col));
          }
        }

        return;
      }
      /***** Archon::DeInterlace::nirc2 ***************************************/


      /***** Archon::DeInterlace::none ****************************************/
      /**
       * @brief      no deinterlacing -- copy imbuf to workbuf
       * @param[in]  bufrows
       * @details 
       *
       * ~~~
       *    +-------------------+
       *    |                   |
       *    |                   |
       *    |         0         |
       *    | <---------------- |
       * L1 +-------------------+
       * ~~~
       *
       */
      void none( int bufrows ) {
        std::string function = "Archon::DeInterlace::none";
        std::stringstream message;

        for ( int r=0, index=0; r<bufrows; r++ ) {
          for ( int c=0; c<this->cols; c++ ) {
            int loc = (r * this->cols) + c ;
            *( this->workbuf + loc ) = *( this->imbuf + (index++) );
/***
            // Store some special pixels for debugging
            //
            if ( loc == (2048*511) +    0          ) *( this->workbuf + loc ) = 20111;
            if ( loc == (2048*511) +  512          ) *( this->workbuf + loc ) = 20222;
            if ( loc == (2048*511) + 1024          ) *( this->workbuf + loc ) = 20333;
            if ( loc == (2048*511) + 1536          ) *( this->workbuf + loc ) = 20444;
            if ( loc == (2048*511) +    0 +2048*512) *( this->workbuf + loc ) = 30111;
            if ( loc == (2048*511) +  512 +2048*512) *( this->workbuf + loc ) = 30222;
            if ( loc == (2048*511) + 1024 +2048*512) *( this->workbuf + loc ) = 30333;
            if ( loc == (2048*511) + 1536 +2048*512) *( this->workbuf + loc ) = 30444;
***/
          }
        }
        return;
      }
      /***** Archon::DeInterlace::none ****************************************/


    public:

      /***** Archon::DeInterlace::DeInterlace *********************************/
      /**
       * @brief      class constructor
       * @param[in]  imbuf_in      T* pointer to image buffer (this will be image_data)
       * @param[in]  workbuf_in    T* pointer to work buffer (this will be where the deinterlacing is performed)
       * @param[in]  bufsize         
       * @param[in]  cols          
       * @param[in]  rows          
       * @param[in]  readout_type  
       *
       */
      DeInterlace( T* imbuf_in, T* workbuf_in, T* cdsbuf_in, bool iscds, long bufsize, int cols, int rows, int readout_type, long height, long width, long depth ) {
        this->imbuf = imbuf_in;
        this->workbuf = workbuf_in;
        this->cdsbuf = cdsbuf_in;
        this->iscds = iscds;
        this->bufsize = bufsize;
        this->cols = cols;
        this->rows = rows;
        this->readout_type = readout_type;
        this->frame_rows = height;
        this->frame_cols = width;
        this->cubedepth = depth;
      }
      /***** Archon::DeInterlace::DeInterlace *********************************/


      /***** Archon::DeInterlace::info ****************************************/
      /**
       * @brief returns some info, just for debugging
       *
       */
      std::string info() {
        std::stringstream ret;
        ret << " imbuf=" << std::hex << this->imbuf << " this->workbuf=" << std::hex << this->workbuf
            << " bufsize=" << std::dec << this->bufsize << " cols=" << this->cols << " rows=" << this->rows
            << " readout_type=" << this->readout_type;
        return ( ret.str() );
      }
      /***** Archon::DeInterlace::info ****************************************/


      /***** Archon::DeInterlace::do_deinterlace ******************************/
      /**
       * @brief
       *
       * This calls the appropriate deinterlacing function based on the readout
       * type, which is an enum that was given to the class constructor when
       * this DeInterlace object was constructed.
       *
       */
      void do_deinterlace( int bufrows ) {
        std::string function = "Archon::DeInterlace::do_deinterlace";
        std::stringstream message;

        switch( this->readout_type ) {
          case Archon::NONE:
            this->none( bufrows );
            break;
          case Archon::NIRC2:
            this->nirc2( bufrows );
            break;
          case Archon::NIRC2VIDEO:
            this->nirc2_video( bufrows );
            break;
          case Archon::TEST:
            this->test( bufrows );
            break;
          default:
            message.str(""); message << "ERROR: unknown readout type: " << this->readout_type;
            logwrite( function, message.str() );
        }

        return;
      }
      /***** Archon::DeInterlace::do_deinterlace ******************************/


  };
  /***** Archon::DeInterlace **************************************************/


  /***** Archon::Interface ****************************************************/
  /**
   * @class  Interface
   * @brief  This class defines the interface to the Archon controller
   *
   */
  class Interface {
    private:
      unsigned long int start_timer, finish_timer;  //!< Archon internal timer, start and end of exposure
      int n_hdrshift;                               //!< number of right-shift bits for Archon buffer in HDR mode
      struct timespec cal_systime;
      unsigned long int cal_archontime;

    public:
      Interface();
      ~Interface();

      // Class Objects
      //
      Network::TcpSocket archon;             /// this is how we talk to the Archon
      Camera::Information camera_info;       /// this is the main camera_info object
      Camera::Information cds_info;          /// this is the main camera_info object
//    Camera::Information fits_info;         /// used to copy the camera_info object to preserve info for FITS writing  //TODO @todo obsolete?
      Camera::Camera camera;                 /// instantiate a Camera object
      Common::FitsKeys userkeys;             /// instantiate a FitsKeys object for user-defined keywords
      Common::FitsKeys systemkeys;           /// instantiate a FitsKeys object for system-defined keywords
      Common::FitsKeys extkeys;              /// instantiate a FitsKeys object for extension-only keywords

      Config config;

      FITS_file fits_file;                   //!< instantiate a FITS container object
      FITS_file cds_file;                    //!< instantiate a FITS container object

      typedef struct {
        ReadoutType readout_type;            //!< enum for readout type
        uint32_t    readout_arg;             //!< future use?
      } readout_info_t;

      std::map< std::string, readout_info_t > readout_source;  //!< STL map of readout sources indexed by readout name

      std::atomic<int> deinterlace_count;     /// number of times deinterlace has been called when mex=true
      std::atomic<int> write_frame_count;     /// number of times write_frame() has been called when mex=true

      std::mutex deinter_mtx;                 /// deinterlacing mutex
      std::condition_variable deinter_cv;     /// deinterlacing condition variable
      std::vector<bool> ringbuf_deinterlaced; /// set if this ring buffer been deinterlaced

      /**
       * @var     ringlock
       * @brief   vector of flags which indicate if the ring buffer is locked for writing
       * @details The ring buffer is flagged as locked while read_frame() is reading the Archon
       *          frame buffer into it.  Note that you can't have a vector of atomics but you
       *          can have a vector of unique_ptrs that point to atomic.
       */
      std::vector<std::unique_ptr<std::atomic<bool>>> ringlock;

      int  msgref;                           //!< Archon message reference identifier, matches reply to command
      bool abort;
      int  taplines;
      std::vector<int> gain;                 //!< digital CDS gain (from TAPLINE definition)
      std::vector<int> offset;               //!< digital CDS offset (from TAPLINE definition)
      bool modeselected;                     //!< true if a valid mode has been selected, false otherwise
      bool firmwareloaded;                   //!< true if firmware is loaded, false otherwise
      bool is_longexposure;                  //!< true for long exposure mode (exptime in sec), false for exptime in msec
      uint32_t readout_arg;

      bool lastmexamps;

      std::string trigin_state;              //!< for external triggering of exposures

      int trigin_expose;                     //!< current value of trigin expose
      int trigin_expose_enable;              //!< value which enables trigin expose
      int trigin_expose_disable;             //!< value which disables trigin expose

      int trigin_untimed;                    //!< current value of trigin_untimed
      int trigin_untimed_enable;             //!< value which enables trigin untimed
      int trigin_untimed_disable;            //!< value which disables trigin untimed

      int trigin_readout;                    //!< current value of trigin_readout
      int trigin_readout_enable;             //!< value which enables trigin readout
      int trigin_readout_disable;            //!< value which disables trigin readout

      std::string trigin_exposeparam;        //!< parameter name to write for trigin_expose
      std::string trigin_untimedparam;       //!< parameter name to write for trigin_untimed
      std::string trigin_readoutparam;       //!< parameter name to write for trigin_readout

      float heater_target_min;               //!< minimum heater target temperature
      float heater_target_max;               //!< maximum heater target temperature

      char *image_data;                      //!< image data buffer //TODO @todo obsolete?
      int ringcount;
      std::vector<char*> image_ring;
      std::vector<void*> work_ring;
      std::vector<void*> cds_ring;
      std::vector<uint32_t> ringdata_allocated;

      void *workbuf;                         //!< pointer to workspace for performing deinterlacing //TODO @todo obsolete?
      long workbuf_size;
      long cdsbuf_size;
      uint32_t image_data_bytes;             //!< requested number of bytes allocated for image_data rounded up to block size
      uint32_t image_data_allocated;         //!< allocated number of bytes for image_data

      std::atomic<bool> openfits_error;      //!< indicates the openfits thread had an error (or not)
      std::atomic<bool> archon_busy;         //!< indicates a thread is accessing Archon
      std::mutex archon_mutex;               //!< protects Archon from being accessed by multiple threads,
                                             //!< use in conjunction with archon_busy flag
      std::string exposeparam;               //!< param name to trigger exposure when set =1
      std::string abortparam;                //!< param name to trigger an abort when set =1
      std::string mcdspairs_param;           //!< param name to set MCDS samples
      std::string mcdsmode_param;            //!< param name to set MCDS mode
      std::string rxmode_param;              //!< param name to set RX mode (read-reset video)
      std::string rxrmode_param;             //!< param name to set RXR mode (read-reset-read video)
      std::string videosamples_param;        //!< param name to set video samples for RX, RXR modes
      std::string utrmode_param;             //!< param name to set UTR mode
      std::string utrsamples_param;          //!< param name to set UTR samples

      std::string shutenableparam;           //!< param name to enable shutter open on expose
      int shutenable_enable;                 //!< the value which enables shutter enable
      int shutenable_disable;                //!< the value which disables shutter enable

      void inc_ringcount() { if (++this->ringcount == IMAGE_RING_BUFFER_SIZE) this->ringcount=0; }

      // Functions
      //
      long abort_archon();                   //!< set the abort parameter on the Archon
      long caltimer();                       //!< calibrate Archon timer
      long interface(std::string &iface);    //!< get interface type
      long configure_controller();           //!< get configuration parameters
      long prepare_image_buffer();           //!< prepare image_data, allocating memory as needed
      long prepare_ring_buffer();            //!< prepare image_data, allocating memory as needed
      long connect_controller(std::string devices_in);  //!< open connection to archon controller
      long disconnect_controller();          //!< disconnect from archon controller
      long cleanup_memory();                 //!< free allocated memory
      long load_timing(std::string acffile); //!< load specified ACF then LOADTIMING
      long load_timing(std::string acffile, std::string &retstring);
      long load_firmware(std::string acffile); //!< load specified acf then APPLYALL
      long load_firmware(std::string acffile, std::string &retstring);
      long load_acf(std::string acffile);    //!< only load (WCONFIG) the specified ACF file
      long set_camera_mode(std::string mode_in);
      long load_mode_settings(std::string mode);
      long native(std::string cmd);
      long archon_cmd(std::string cmd);
      long archon_cmd(std::string cmd, std::string &reply);
      long read_parameter(std::string paramname, std::string &valstring);
      long prep_parameter(std::string paramname, std::string value);
      long load_parameter(std::string paramname, std::string value);
      long fetchlog();
      long get_frame_status();
      void print_frame_status();
      long lock_buffer(int buffer);
      long get_timer(unsigned long int *timer);
      long fetch(uint64_t bufaddr, uint32_t bufblocks);
      long read_frame();                     //!< read Archon frame buffer into host memory
      long read_frame(Camera::frame_type_t frame_type); /// read Archon frame buffer into host memory
      long read_frame( Camera::frame_type_t frame_type, char* &ptr );
      long read_frame( Camera::frame_type_t frame_type, char* &ptr, int ringcount_in );
      long write_frame();                    //!< write (a previously read) Archon frame buffer to disk
      long write_frame(int ringcount_in);    //!< write (a previously read) Archon frame buffer to disk
      long write_raw();                      //!< write raw 16 bit data to a FITS file
      long write_config_key( const char *key, const char *newvalue, bool &changed );
      long write_config_key( const char *key, int newvalue, bool &changed );
      long write_parameter( const char *paramname, const char *newvalue, bool &changed );
      long write_parameter( const char *paramname, int newvalue, bool &changed );
      long write_parameter( const char *paramname, const char *newvalue );
      long write_parameter( const char *paramname, int newvalue );
      template <class T> long get_configmap_value(std::string key_in, T& value_out);
      void add_filename_key();
      void add_filename_key( Camera::Information &info );  /// adds the current filename to the systemkeys database
      long poweron();
      long poweroff();
      long expose( std::string nseq_in );
      long do_expose(std::string nseq_in);
      long wait_for_exposure();
      long wait_for_readout();
      long get_parameter(std::string parameter, std::string &retstring);
      long get_parammap_value( std::string param_in, long& value_out );
      long set_parameter( std::string parameter, long value );
      long set_parameter(std::string parameter);
      long exptime(std::string exptime_in, std::string &retstring);
      void copy_keydb();                                                /// copy user keyword database into camera_info
      long longexposure(std::string state_in, std::string &state_out);
      long shutter(std::string shutter_in, std::string& shutter_out);
      long hdrshift(std::string bits_in, std::string &bits_out);
      long trigin(std::string state_in);
      long heater(std::string args, std::string &retstring);
      long sensor(std::string args, std::string &retstring);
      long bias(std::string args, std::string &retstring);
      long cds(std::string args, std::string &retstring);
      long inreg( std::string args );
      long coadd( std::string coadds_in, std::string &retstring );
      void make_camera_header();
      long recalc_geometry();
      long region_of_interest( std::string args, std::string &retstring );
      long sample_mode( std::string args, std::string &retstring );
      long readout( std::string readout_in, std::string &readout_out );
      long test(std::string args, std::string &retstring);

      static void dothread_runmcdsproc( Interface *self );
      static void dothread_runcds( Interface *self );                             /// this is run in a thread to open a fits file for flat (non-mex) files only
      static void dothread_openfits( Interface *self );                             /// this is run in a thread to open a fits file for flat (non-mex) files only
      static void dothread_writeframe( Interface *self, int ringcount_in );         /// this is run in a thread to write a frame after it is deinterlaced
      static void dothread_start_deinterlace( Interface *self, int ringcount_in );  /// calls the appropriate deinterlacer based on camera_info.datatype

      long alloc_workbuf();
      template <class T> void* alloc_workbuf(T* buf);
      template <class T> void free_workbuf(T* buf);

      long alloc_workring();
      template <class T> void  alloc_workring( T* buf );
      template <class T> void  free_workring( T* buf );

      template <class T> T* deinterlace( T* imbuf, T* workbuf, T* cdsbuf, int ringcount_in );
      template <class T> static void dothread_deinterlace( Interface *self, DeInterlace<T> &deinterlace, int bufcols, int bufrows, int ringcount );

      /**
       * @var     struct geometry_t geometry[]
       * @details structure of geometry which is unique to each observing mode
       */
      struct geometry_t {
        int  amps[2];              // number of amplifiers per detector for each axis, set in set_camera_mode
        int  num_detect;           // number of detectors, set in set_camera_mode
        int  linecount;            // number of lines per tap
        int  pixelcount;           // number of pixels per tap
        int  framemode;            // Archon deinterlacing mode, 0=topfirst, 1=bottomfirst, 2=split
      };

      /**
       * @var     struct tapinfo_t tapinfo[]
       * @details structure of tapinfo which is unique to each observing mode
       */
      struct tapinfo_t {
        int   num_taps;
        int   tap[16];
        float gain[16];
        float offset[16];
        std::string readoutdir[16];
      };

      /**
       * @var     struct frame_data_t frame
       * @details structure to contain Archon results from "FRAME" command
       */
      struct frame_data_t {
        int      index;                       // index of newest buffer data
        int      frame;                       // frame of newest buffer data
        int      next_index;                  // index of next buffer
        std::string timer;                    // current hex 64 bit internal timer
        int      rbuf;                        // current buffer locked for reading
        int      wbuf;                        // current buffer locked for writing
        std::vector<int>      bufsample;      // sample mode 0=16 bit, 1=32 bit
        std::vector<int>      bufcomplete;    // buffer complete, 1=ready to read
        std::vector<int>      bufmode;        // buffer mode: 0=top 1=bottom 2=split
        std::vector<uint64_t> bufbase;        // buffer base address for fetching
        std::vector<int>      bufframen;      // buffer frame number
        std::vector<int>      bufwidth;       // buffer width
        std::vector<int>      bufheight;      // buffer height
        std::vector<int>      bufpixels;      // buffer pixel progress
        std::vector<int>      buflines;       // buffer line progress
        std::vector<int>      bufrawblocks;   // buffer raw blocks per line
        std::vector<int>      bufrawlines;    // buffer raw lines
        std::vector<int>      bufrawoffset;   // buffer raw offset
        std::vector<uint64_t> buftimestamp;   // buffer hex 64 bit timestamp
        std::vector<uint64_t> bufretimestamp; // buf trigger rising edge time stamp
        std::vector<uint64_t> buffetimestamp; // buf trigger falling edge time stamp
      } frame;

      /** @var      vector modtype
       *  @details  stores the type of each module from the SYSTEM command
       */
      std::vector<int> modtype;

      /** @var      vector modversion
       *  @details  stores the version of each module from the SYSTEM command
       */
      std::vector<std::string> modversion;

      std::string backplaneversion;

      /** @var      int lastframe
       *  @details  the last (I.E. previous) frame number acquired
       */
      int lastframe;

      /**
       * rawinfo_t is a struct which contains variables specific to raw data functions
       */
      struct rawinfo_t {
        int adchan;          // selected A/D channels
        int rawsamples;      // number of raw samples per line
        int rawlines;        // number of raw lines
        int iteration;       // iteration number
        int iterations;      // number of iterations
      } rawinfo;

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      typedef struct {
        int    line;           // the line number, used for updating Archon
        std::string value;     // used for configmap
      } config_line_t;

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      typedef struct {
        std::string key;       // the PARAMETERn part
        std::string name;      // the parametername part
        std::string value;     // the value part
        int    line;           // the line number, used for updating Archon
      } param_line_t;

      typedef std::map<std::string, config_line_t>  cfg_map_t;
      typedef std::map<std::string, param_line_t>   param_map_t;

      cfg_map_t   configmap;
      param_map_t parammap;

      /** 
       * \var     modeinfo_t modeinfo
       * \details structure contains a configmap and parammap unique to each mode,
       *          specified in the [MODE_*] sections at the end of the .acf file.
       */
      typedef struct {
        int         rawenable;     //!< initialized to -1, then set according to RAWENABLE in .acf file
        cfg_map_t   configmap;     //!< key=value map for configuration lines set in mode sections
        param_map_t parammap;      //!< PARAMETERn=parametername=value map for mode sections
        Common::FitsKeys acfkeys;  //!< create a FitsKeys object to hold user keys read from ACF file for each mode
        geometry_t  geometry;
        tapinfo_t   tapinfo;
      } modeinfo_t;

      std::map<std::string, modeinfo_t> modemap;

      /**
       * generic key=value STL map for Archon commands
       */
      typedef std::map<std::string, std::string> map_t;

      /**
       * \var     map_t systemmap
       * \details key=value map for Archon SYSTEM command
       */
      map_t systemmap;

      /**
       * \var     map_t statusmap
       * \details key=value map for Archon STATUS command
       */
      map_t statusmap;

  };
  /***** Archon::Interface ****************************************************/


}
#endif
