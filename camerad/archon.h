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
    TEST,
    NUM_READOUT_TYPES
    };


  const int IMAGE_RING_BUFFER_SIZE = 5;

  /***** Archon::DeInterlace **************************************************/
  /**
   * @class DeInterlace
   * @brief This is the deinterlacing class
   *
   */
  template <typename T>
  class DeInterlace {
    public:
    private:
      T* workbuf;
      T* imbuf;
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
       * @param[in] row_start
       * @param[in] row_stop
       * @param[in] index
       * @details 
       *
       */
      void test( int row_start, int row_stop, int index ) {

        for (long pix=0; pix < 256*256; pix++) {
          this->imbuf[pix] = pix % 65535;
        }

        cv::Mat image;

        image = cv::Mat( 256, 256, CV_16U, this->imbuf );

        return;
      }
      /***** Archon::DeInterlace::test ****************************************/


      /***** Archon::DeInterlace::nirc2 ***************************************/
      /**
       * @brief     NIRC2 deinterlacing
       * @param[in] row_start
       * @param[in] row_stop
       * @param[in] index
       * @details 
       *
       * ~~~
       *                                               Q1 +---------+---------+ Q2
       * +---------+---------+---------+---------+        | <------ | ------> |
       * |         |         |         |         |        |       ^ | ^       |
       * |   Q1    |   Q2    |   Q3    |   Q4    | ===>   +---------+---------+
       * |         |         |         |         |        |       v | v       |
       * +---------+---------+---------+---------+        | <------ | ------> |
       *                                               Q3 +---------+---------+ Q4
       * ~~~
       *
       */
      void nirc2( int row_start, int row_stop, int index, int ringcount_in ) {
#ifdef LOGLEVEL_DEBUG
        std::stringstream message;
        message.str(""); message << "[DEBUG] row_start=" << row_start << " row_stop=" << row_stop << " this->rows=" << this->rows << " this->cols=" << this->cols << " rincount_in=" << ringcount_in;
        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif

        int taps=8;

        int quad_rows  = this->frame_rows / 2;
        int quad_cols  = this->frame_cols / 2;

        // Create openCV image to hold entire imbuf (all cubes)
        //
        cv::Mat image = cv::Mat( row_stop, this->cols, CV_16U, this->imbuf );

        // Create an empty openCV image to hold the final, deinterlaced, re-assembled image
        //
        cv::Mat work = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );

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
        for ( int i=0, j=1, workindex=0; i < this->cubedepth; i++, j++ ) {
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

          // Copy assembled image into the FITS buffer, this->workbuf
          //
          for ( int row=0; row<this->frame_rows; row++ ) {
            for ( int col=0; col<this->frame_cols; col++ ) {
              *( this->workbuf + workindex++ ) = 65535 - (uint16_t)(work.at<uint16_t>(row,col) - 0);
            }
          }
        }

        return;
      }
      /***** Archon::DeInterlace::nirc2 ***************************************/


      /***** Archon::DeInterlace::none ****************************************/
      /**
       * @brief     no deinterlacing -- copy imbuf to workbuf
       * @param[in] row_start
       * @param[in] row_stop
       * @param[in] index
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
      void none( int row_start, int row_stop, int index ) {
        std::string function = "Archon::DeInterlace::none";
        std::stringstream message;

        for ( int r=row_start; r<row_stop; r++ ) {
          for ( int c=0; c<this->cols; c++ ) {
            int loc = (r * this->cols) + c ;
            *( this->workbuf + loc ) = *( this->imbuf + (index++) );
/***
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
      DeInterlace( T* imbuf_in, T* workbuf_in, long bufsize, int cols, int rows, int readout_type, long height, long width, long depth ) {
        this->imbuf = imbuf_in;
        this->workbuf = workbuf_in;
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
       * The deinterlacing is performed from "row_start" to "row_stop" of the
       * final image, using the pixel "index" of the raw image buffer.
       *
       */
      void do_deinterlace( int row_start, int row_stop, int index, int index_flip, int ringcount_in ) {
        std::string function = "Archon::DeInterlace::do_deinterlace";
        std::stringstream message;

        switch( this->readout_type ) {
          case Archon::NONE:
            this->none( row_start, row_stop, index );
            break;
          case Archon::NIRC2:
            this->nirc2( row_start, row_stop, index, ringcount_in );
            break;
          case Archon::TEST:
            this->test( row_start, row_stop, index );
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


  class Interface {
    private:
      unsigned long int start_timer, finish_timer;  //!< Archon internal timer, start and end of exposure
      int n_hdrshift;                               //!< number of right-shift bits for Archon buffer in HDR mode

    public:
      Interface();
      ~Interface();

      // Class Objects
      //
      Network::TcpSocket archon;
      Camera::Information camera_info;       /// this is the main camera_info object
      Camera::Information fits_info;         /// used to copy the camera_info object to preserve info for FITS writing
      Camera::Camera camera;                 /// instantiate a Camera object
      Common::FitsKeys userkeys;             //!< instantiate a Common object
      Common::FitsKeys systemkeys;           //!< instantiate a Common object

      Config config;

      FITS_file fits_file;                   //!< instantiate a FITS container object

      typedef struct {
        ReadoutType readout_type;            //!< enum for readout type
        uint32_t    readout_arg;             //!< future use?
      } readout_info_t;

      std::map< std::string, readout_info_t > readout_source;  //!< STL map of readout sources indexed by readout name

std::mutex mtx;
std::condition_variable cv;
std::vector<bool> ringbuf_deinterlaced;
// can't have a vector of atomics but you can have a vector of unique_ptrs that point to atomic
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

      char *image_data;                      //!< image data buffer
      int ringcount;
      std::vector<char*> image_ring;
      std::vector<void*> work_ring;
      std::vector<uint32_t> ringdata_allocated;

      void *workbuf;                         //!< pointer to workspace for performing deinterlacing
      long workbuf_size;
      uint32_t image_data_bytes;             //!< requested number of bytes allocated for image_data rounded up to block size
      uint32_t image_data_allocated;         //!< allocated number of bytes for image_data

      std::atomic<bool> openfits_error;      //!< indicates the openfits thread had an error (or not)
      std::atomic<bool> archon_busy;         //!< indicates a thread is accessing Archon
      std::mutex archon_mutex;               //!< protects Archon from being accessed by multiple threads,
                                             //!< use in conjunction with archon_busy flag
      std::string exposeparam;               //!< param name to trigger exposure when set =1
      std::string mcdspairs_param;           //!< param name to set MCDS samples
      std::string mcdsmode_param;            //!< param name to set MCDS mode
      std::string utrmode_param;             //!< param name to set UTR mode
      std::string utrsamples_param;          //!< param name to set UTR samples

      std::string shutenableparam;           //!< param name to enable shutter open on expose
      int shutenable_enable;                 //!< the value which enables shutter enable
      int shutenable_disable;                //!< the value which disables shutter enable

      void inc_ringcount() { if (++this->ringcount == IMAGE_RING_BUFFER_SIZE) this->ringcount=0; }

      // Functions
      //
      long interface(std::string &iface);    //!< get interface type
      long configure_controller();           //!< get configuration parameters
      long prepare_image_buffer();           //!< prepare image_data, allocating memory as needed
      long prepare_ring_buffer();            //!< prepare image_data, allocating memory as needed
      long connect_controller(std::string devices_in);  //!< open connection to archon controller
      long disconnect_controller();          //!< disconnect from archon controller
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
      long region_of_interest( std::string args, std::string &retstring );
      long sample_mode( std::string args, std::string &retstring );
      long readout( std::string readout_in, std::string &readout_out );
      long test(std::string args, std::string &retstring);

      static void dothread_openfits( Interface *self );
//    static void dothread_openfits( Interface *self, FITS_file &fits_file, Camera::Information &info, Camera::Camera &camera );
//    static void dothread_writeframe( Interface *self, FITS_file &fits_file, Camera::Information &info, Camera::Camera &camera );
//    static void dothread_writeframe( Interface *self );
      static void dothread_writeframe( Interface *self, int ringcount_in );
      static void dothread_start_deinterlace( Interface *self, int ringcount_in );

      long alloc_workbuf();
      template <class T> void* alloc_workbuf(T* buf);
      template <class T> void  alloc_workring(T* buf);
      template <class T> void free_workbuf(T* buf);
      template <class T> T* deinterlace( T* imbuf, T* workbuf, int ringcount_in );
//    template <class T> static void dothread_deinterlace( DeInterlace<T> &deinterlace, Interface *self, int bufcols, int bufrows, int section, int nthreads );
      template <class T> static void dothread_deinterlace( Interface *self, DeInterlace<T> &deinterlace, int bufcols, int bufrows, int section, int ringcount, int nthreads );

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


}
#endif
