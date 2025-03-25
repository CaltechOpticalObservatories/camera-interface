/**
 * @file    archon.cpp
 * @brief   camera interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "archon.h"

namespace Archon {

  /***** Archon::Interface::Interface *****************************************/
  /**
   * @brief      Archon Interface constructor
   *
   */
  Interface::Interface() {
    this->openfits_error = false;
    this->modeselected = false;
    this->firmwareloaded = false;
    this->msgref = 0;
    this->lastframe = 0;
    this->frame.index = 0;
    this->frame.next_index = 0;
    this->taplines = 0;
    this->ringcount = 0;
    this->ringbuf_deinterlaced.reserve( Archon::IMAGE_RING_BUFFER_SIZE );
    this->image_ring.reserve( Archon::IMAGE_RING_BUFFER_SIZE );
    this->work_ring.reserve( Archon::IMAGE_RING_BUFFER_SIZE );
    this->cds_ring.reserve( Archon::IMAGE_RING_BUFFER_SIZE );
    for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
      this->image_ring.push_back(nullptr);
      this->work_ring.push_back(nullptr);
      this->cds_ring.push_back(nullptr);
      this->ringdata_allocated.push_back(0);
      this->ringbuf_deinterlaced.push_back(false);
    }

    this->ringlock.resize( Archon::IMAGE_RING_BUFFER_SIZE );  // pre-size the ringlock container

    this->deinterlace_count.store( 0, std::memory_order_seq_cst );
    this->write_frame_count.store( 0, std::memory_order_seq_cst );

    // Can't have a vector of atomics but can have a vector of unique_ptr.
    // Initialize those pointers here.
    //
    for ( auto &p : this->ringlock ) {
        p = std::make_unique<std::atomic<bool>>(false);
    }

    this->coaddbuf = nullptr;
    this->mcdsbuf_0 = nullptr;
    this->mcdsbuf_1 = nullptr;
    this->image_data = nullptr;
    this->image_data_bytes = 0;
    this->image_data_allocated = 0;
    this->workbuf = nullptr;
    this->workbuf_size = 0;
    this->cdsbuf_size = 0;
    this->is_longexposure = false;
    this->n_hdrshift = 0;
    this->backplaneversion="";

    this->write_tapinfo_to_fits = true;  // gains and offsets go in FITS headers, can be overridden by config file

    this->shutenable_enable      = DEF_SHUTENABLE_ENABLE;
    this->shutenable_disable     = DEF_SHUTENABLE_DISABLE;

    // Initialize STL map of Readout Amplifiers
    // Indexed by amplifier name.
    // The number is the argument for the Arc command to set this amplifier in the firmware.
    //
    this->readout_source.insert( { "NONE",       { READOUT_NONE,       0 } } );
    this->readout_source.insert( { "NIRC2",      { READOUT_NIRC2,      0 } } );
    this->readout_source.insert( { "NIRC2VIDEO", { READOUT_NIRC2VIDEO, 0 } } );
    this->readout_source.insert( { "TEST",       { READOUT_TEST,       0 } } );

    // pre-size the modtype and modversion vectors to hold the max number of modules
    //
    this->modtype.resize( nmods );
    this->modversion.resize( nmods );

    // TODO I should change these to STL maps instead
    //
    this->frame.bufsample.resize( Archon::nbufs );
    this->frame.bufcomplete.resize( Archon::nbufs );
    this->frame.bufmode.resize( Archon::nbufs );
    this->frame.bufbase.resize( Archon::nbufs );
    this->frame.bufframen.resize( Archon::nbufs );
    this->frame.bufwidth.resize( Archon::nbufs );
    this->frame.bufheight.resize( Archon::nbufs );
    this->frame.bufpixels.resize( Archon::nbufs );
    this->frame.buflines.resize( Archon::nbufs );
    this->frame.bufrawblocks.resize( Archon::nbufs );
    this->frame.bufrawlines.resize( Archon::nbufs );
    this->frame.bufrawoffset.resize( Archon::nbufs );
    this->frame.buftimestamp.resize( Archon::nbufs );
    this->frame.bufretimestamp.resize( Archon::nbufs );
    this->frame.buffetimestamp.resize( Archon::nbufs );
  }
  /***** Archon::Interface::Interface *****************************************/


  /***** Archon::Interface::~Interface ****************************************/
  /**
   * @brief      Archon Interface deconstructor
   * @details    frees up memory on close
   *
   */
  Interface::~Interface() {

    if (this->image_data != nullptr) { delete [] this->image_data; this->image_data=nullptr; }

    for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
      if ( this->image_ring.at(i) != nullptr ) {
        delete [] this->image_ring.at(i);
        this->image_ring.at(i) = nullptr;
      }
    }

    {
    void* ptr=nullptr;
    switch ( this->camera_info.datatype ) {
      case USHORT_IMG: {
        this->free_workring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->free_workring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->free_workring( (uint32_t *)ptr );
        break;
      }
      case 0:   // not set
        break;
      default:
        std::stringstream message;
        message << "cannot free work_ring for unknown datatype: " << this->camera_info.datatype;
        this->camera.log_error( "Interface::~Interface", message.str() );
        break;
    }
    }

    {
    void* ptr=nullptr;
    switch ( this->cds_info.datatype ) {
      case USHORT_IMG: {
        this->free_cdsring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->free_cdsring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->free_cdsring( (uint32_t *)ptr );
        break;
      }
      case LONG_IMG: {
        this->free_cdsring( (int32_t *)ptr );
        break;
      }
      case 0:   // not set
        break;
      default:
        std::stringstream message;
        message << "cannot free cds_ring for unknown datatype: " << this->cds_info.datatype;
        this->camera.log_error( "Interface::~Interface", message.str() );
        break;
    }
    }
  }
  /***** Archon::Interface::~Interface ****************************************/


  /**************** Archon::Interface::interface ******************************/
  long Interface::interface(std::string &iface) {
    const std::string function("Archon::Interface::interface");
    iface = "STA-Archon";
    logwrite(function, iface);
    return(0);
  }
  /**************** Archon::Interface::interface ******************************/


  /**************** Archon::Interface::configure_controller *******************/
  /**
   * @fn     configure_controller
   * @brief  get controller-specific values out of the configuration file
   * @param  none
   * @return NO_ERROR if successful or ERROR on error
   *
   */
  long Interface::configure_controller() {
    const std::string function("Archon::Interface::configure_controller");
    std::stringstream message;
    int applied=0;
    long error;

    // Must re-init all values to start-up defaults in case this function is 
    // called again to re-load the config file (such as if a HUP is received)
    // and the new config file may not have everything defined.
    // This ensures nothing is carried over from any previous config.
    //
    this->archon.sethost( "" );
    this->archon.setport( -1 );

    this->is_longexposure = false;
    this->n_hdrshift = 0;

    this->camera.firmware[0] = "";

    this->mcdspairs_param = "";
    this->mcdsmode_param = "";
    this->rxmode_param = "";
    this->rxrmode_param = "";
    this->videosamples_param = "";
    this->utrsamples_param = "";
    this->utrmode_param = "";
    this->abortparam = "";
    this->exposeparam = "";

    this->shutenable_enable      = DEF_SHUTENABLE_ENABLE;
    this->shutenable_disable     = DEF_SHUTENABLE_DISABLE;

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      // ARCHON_IP is used to set the Archon host name in the Network::TcpSocket archon object
      //
      if (config.param[entry]=="ARCHON_IP") {
        this->archon.sethost( config.arg[entry] );
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      // ARCHON_PORT is used to set the Archon port in the Network::TcpSocket archon object
      //
      if (config.param[entry]=="ARCHON_PORT") {
        int port;
        try {
          port = std::stoi( config.arg[entry] );
        }
        catch (std::exception &e) {
          message.str(""); message << "parsing ARCHON_PORT number: " << e.what();
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        this->archon.setport(port);
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 21, "WRITE_TAPINFO_TO_FITS")==0) {    // WRITE_TAPINFO_TO_FITS
        if ( config.arg[entry] == "no" ) {
          this->write_tapinfo_to_fits = false;
        }
        else
        if ( config.arg[entry] == "yes" ) {
          this->write_tapinfo_to_fits = true;
        }
        else {
          message.str(""); message << "NOTICE: unrecognized value \"" << config.arg[entry] << "\" for WRITE_TAPINFO_TO_FITS."
                                   << " Default is yes.";
          this->write_tapinfo_to_fits = true;
          this->camera.async.enqueue( message.str() );
          logwrite( function, message.str() );
        }
        applied++;
      }

      if (config.param[entry].compare(0, 15, "MCDSPAIRS_PARAM")==0) {          // MCDSPAIRS_PARAM
        this->mcdspairs_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 14, "MCDSMODE_PARAM")==0) {           // MCDSMODE_PARAM
        this->mcdsmode_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 12, "RXMODE_PARAM")==0) {             // RXMODE_PARAM
        this->rxmode_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 13, "RXRMODE_PARAM")==0) {            // RXRMODE_PARAM
        this->rxrmode_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 18, "VIDEOSAMPLES_PARAM")==0) {       // VIDEOSAMPLES_PARAM
        this->videosamples_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 15, "UTRSAMPLE_PARAM")==0) {          // UTRSAMPLE_PARAM
        this->utrsamples_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 13, "UTRMODE_PARAM")==0) {            // UTRMODE_PARAM
        this->utrmode_param = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 11, "ABORT_PARAM")==0) {             // ABORT_PARAM
        this->abortparam = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 12, "EXPOSE_PARAM")==0) {             // EXPOSE_PARAM
        this->exposeparam = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 16, "SHUTENABLE_PARAM")==0) {         // SHUTENABLE_PARAM
        this->shutenableparam = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 17, "SHUTENABLE_ENABLE")==0) {        // SHUTENABLE_ENABLE
        int enable;
        try {
          enable = std::stoi( config.arg[entry] );
	}
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert SHUTENABLE_ENABLE to integer" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "SHUTENABLE_ENABLE out of integer range" );
          return(ERROR);
        }
        this->shutenable_enable = enable;
        applied++;
      }

      if (config.param[entry].compare(0, 18, "SHUTENABLE_DISABLE")==0) {       // SHUTENABLE_DISABLE
        int disable;
        try {
          disable = std::stoi( config.arg[entry] );
	}
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert SHUTENABLE_DISABLE to integer" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "SHUTENABLE_DISABLE out of integer range" );
          return(ERROR);
        }
        this->shutenable_disable = disable;
        applied++;
      }

      // .firmware and .readout_time are STL maps but (for now) only one Archon per computer
      // so map always to 0
      //
      if (config.param[entry].compare(0, 16, "DEFAULT_FIRMWARE")==0) {
        this->camera.firmware[0] = config.arg[entry];
        applied++;
      }

      if (config.param[entry].compare(0, 16, "HDR_SHIFT")==0) {
        std::string dontcare;
        this->hdrshift( config.arg[entry], dontcare );
        applied++;
      }

      if (config.param[entry].compare(0, 12, "READOUT_TIME")==0) {
        int readtime;
        try {
          readtime = std::stoi ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert readout time to integer" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "readout time out of integer range" );
          return(ERROR);
        }
        this->camera.readout_time[0] = readtime;
        applied++;
      }

      // DEFAULT_SAMPMODE
      if (config.param[entry].compare(0, 16, "DEFAULT_SAMPMODE")==0) {
        this->camera.default_sampmode = config.arg[entry];
        message.str(""); message << "default_sampmode=" << this->camera.default_sampmode; logwrite( function, message.str() );
        applied++;
      }

      // DEFAULT_EXPTIME
      if (config.param[entry].compare(0, 15, "DEFAULT_EXPTIME")==0) {
        this->camera.default_exptime = config.arg[entry];
        message.str(""); message << "default_exptime=" << this->camera.default_exptime; logwrite( function, message.str() );
        applied++;
      }

      // DEFAULT_ROI
      if (config.param[entry].compare(0, 11, "DEFAULT_ROI")==0) {
        this->camera.default_roi = config.arg[entry];
        message.str(""); message << "default_roi=" << this->camera.default_roi; logwrite( function, message.str() );
        applied++;
      }

      // PIXEL_TIME
      if (config.param[entry].compare(0, 10, "PIXEL_TIME")==0) {
        double pixeltime;
        try {
          pixeltime = std::stod ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert PIXEL_TIME to double" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "PIXEL_TIME out of double range" );
          return(ERROR);
        }
        this->camera_info.pixel_time = pixeltime;
        applied++;
      }

      // PIXEL_SKIP_TIME
      if (config.param[entry].compare(0, 15, "PIXEL_SKIP_TIME")==0) {
        double pixelskiptime;
        try {
          pixelskiptime = std::stod ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert PIXEL_SKIP_TIME to double" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "PIXEL_SKIP_TIME out of double range" );
          return(ERROR);
        }
        this->camera_info.pixel_skip_time = pixelskiptime;
        applied++;
      }

      // ROW_OVERHEAD_TIME
      if (config.param[entry].compare(0, 17, "ROW_OVERHEAD_TIME")==0) {
        double rowoverheadtime;
        try {
          rowoverheadtime = std::stod ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert ROW_OVERHEAD_TIME to double" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "ROW_OVERHEAD_TIME out of double range" );
          return(ERROR);
        }
        this->camera_info.row_overhead_time = rowoverheadtime;
        applied++;
      }

      // ROW_SKIP_TIME
      if (config.param[entry].compare(0, 13, "ROW_SKIP_TIME")==0) {
        double rowskiptime;
        try {
          rowskiptime = std::stod ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert ROW_SKIP_TIME to double" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "ROW_SKIP_TIME out of double range" );
          return(ERROR);
        }
        this->camera_info.row_skip_time = rowskiptime;
        applied++;
      }

      // FRAME_START_TIME
      if (config.param[entry].compare(0, 16, "FRAME_START_TIME")==0) {
        double framestarttime;
        try {
          framestarttime = std::stod ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert FRAME_START_TIME to double" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "FRAME_START_TIME out of double range" );
          return(ERROR);
        }
        this->camera_info.frame_start_time = framestarttime;
        applied++;
      }

      // FS_PULSE_TIME
      if (config.param[entry].compare(0, 13, "FS_PULSE_TIME")==0) {
        double fspulsetime;
        try {
          fspulsetime = std::stod ( config.arg[entry] );
        }
        catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert FS_PULSE_TIME to double" );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "FS_PULSE_TIME out of double range" );
          return(ERROR);
        }
        this->camera_info.fs_pulse_time = fspulsetime;
        applied++;
      }

      if (config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->camera.imdir( config.arg[entry] );
        applied++;
      }

      if (config.param[entry].compare(0, 7, "DIRMODE")==0) {
        std::string s( config.arg[entry] );
        std::stringstream mode_bit;
        mode_t mode=0;
        for ( size_t i=0; i < s.length(); i++ ) {
          try {
            mode = (mode << 3);
            mode_bit.str(""); mode_bit << s.at(i);
            mode |= std::stoi( mode_bit.str() );
          }
          catch (std::invalid_argument &) {
            this->camera.log_error( function, "unable to convert mode bit to integer" );
            return(ERROR);
          }
          catch (std::out_of_range &) {
            this->camera.log_error( function, "out of range converting dirmode bit" );
            return(ERROR);
          }
        }
        this->camera.set_dirmode( mode );
        applied++;
      }

      if (config.param[entry].compare(0, 8, "BASENAME")==0) {
        this->camera.basename( config.arg[entry] );
        applied++;
      }

    }

    message.str("");
    if (applied==0) {
      message << "ERROR: ";
      error = ERROR;
    }
    else {
      error = NO_ERROR;
    }
    message << "applied " << applied << " configuration lines to controller";
    error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() ) ;
    return error;
  }
  /**************** Archon::Interface::configure_controller *******************/


  /**************** Archon::Interface::prepare_ring_buffer ********************/
  /**
   * @brief    prepare image_data buffer, allocating memory as needed
   * @details  This is called once per exposure, by do_expose()
   * @return   NO_ERROR if successful or ERROR on error
   *
   */
  long Interface::prepare_ring_buffer() {
    const std::string function("Archon::Interface::prepare_ring_buffer");
    std::stringstream message;

    // This is the amount of memory to allocate for each fits write.
    // If this is multi-extension (mex) then this is memory per extension.
    // If this is a 3D data cube then this includes the total cube depth.
    //
    uint32_t expected_allocation = this->image_data_bytes * this->camera_info.cubedepth;

    try {
      for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
        // If there is already a correctly-sized buffer allocated,
        // then don't do anything except initialize that space to zero.
        //
        if ( ( this->image_ring.at(i) != nullptr )  &&
             ( expected_allocation != 0 )        &&
             ( this->ringdata_allocated.at(i) == expected_allocation ) ) {
          memset( this->image_ring.at(i), 0, expected_allocation );
          message.str(""); message << "initialized " << expected_allocation << " bytes of ring buffer " << i << " memory at " << (void*)this->image_ring.at(i);
          logwrite(function, message.str());
        }

        // If memory needs to be re-allocated, delete the old buffer
        //
        else {
          if ( this->image_ring.at(i) != nullptr ) {
            message.str(""); message << "deleting ring buffer " << i;
            logwrite( function, message.str() );
            delete [] this->image_ring.at(i);
            this->image_ring.at(i)=nullptr;
          }
          // Allocate new memory
          //
          if (expected_allocation != 0) {
            this->image_ring.at(i) = new char[expected_allocation]{};
            this->ringdata_allocated.at(i)=expected_allocation;
            message.str(""); message << "allocated " << expected_allocation << " bytes for ring buffer " << i << " at " << (void*)this->image_ring.at(i);
            logwrite(function, message.str());
          }
          else {
            this->camera.log_error( function, "cannot allocate zero-length image memory" );
            return(ERROR);
          }
        }
      }
    }
    catch ( std::out_of_range & ) {
      this->camera.log_error( function, "out of range addressing image_ring" );
      return(ERROR);
    }

    return(NO_ERROR);
  }
  /**************** Archon::Interface::prepare_ring_buffer ********************/


  /**************** Archon::Interface::connect_controller *********************/
  /**
   * @fn     connect_controller
   * @brief
   * @param  none (devices_in here for future expansion)
   * @return 
   *
   */
  long Interface::connect_controller(std::string devices_in="") {
    const std::string function("Archon::Interface::connect_controller");
    std::stringstream message;
    int adchans=0;
    long   error = ERROR;

    if ( this->archon.isconnected() ) {
      logwrite(function, "camera connection already open");
      return NO_ERROR;
    }

    // Initialize the camera connection
    //
    logwrite(function, "opening a connection to the camera system");

    if ( this->archon.Connect() != 0 ) {
      message.str(""); message << "connecting to " << this->archon.gethost() << ":" << this->archon.getport() << ": " << strerror(errno);
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    message.str("");
    message << "socket connection to " << this->archon.gethost() << ":" << this->archon.getport() << " "
            << "established on fd " << this->archon.getfd();
    logwrite(function, message.str());

    // Get the current system information for the installed modules
    //
    std::string reply;
    error = this->archon_cmd( SYSTEM, reply );        // first the whole reply in one string

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                    // then each line in a separate token "lines"

    for ( auto line : lines ) {
      Tokenize( line, tokens, "_=" );                 // finally break each line into tokens to get module, type and version
      if ( tokens.size() != 3 ) continue;             // need 3 tokens

      std::string version="";
      int module=0;
      int type=0;

      // get the module number
      //
      if ( tokens[0].compare( 0, 9, "BACKPLANE" ) == 0 ) {
        if ( tokens[1] == "VERSION" ) this->backplaneversion = tokens[2];
        continue;
      }

      // get the module and type of each module from MODn_TYPE
      //
      if ( ( tokens[0].compare( 0, 3, "MOD" ) == 0 ) && ( tokens[1] == "TYPE" ) ) {
        try {
          module = std::stoi( tokens[0].substr(3) );
          type   = std::stoi( tokens[2] );
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert module or type from " << tokens[0] << "=" << tokens[1] << " to integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "module " << tokens[0].substr(3) << " or type " << tokens[1] << " out of range";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
      }
      else continue;

      // get the module version
      //
      if ( tokens[1] == "VERSION" ) version = tokens[2]; else version = "";

      // now store it permanently
      //
      if ( (module > 0) && (module <= nmods) ) {
        try {
          this->modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->modversion.at(module-1) = version;    // store the type in a vector indexed by module
        }
        catch (std::out_of_range &) {
          message.str(""); message << "requested module " << module << " out of range {1:" << nmods;
          this->camera.log_error( function, message.str() );
        }
      }
      else {                                          // else should never happen
        message.str(""); message << "module " << module << " outside range {1:" << nmods << "}";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }

      // Use the module type to resize the gain and offset vectors,
      // but always use the largest possible value allowed.
      //
      if ( type ==  2 ) adchans = ( adchans < MAXADCCHANS ? MAXADCCHANS : adchans );  // ADC module (type=2) found
      if ( type == 17 ) adchans = ( adchans < MAXADMCHANS ? MAXADMCHANS : adchans );  // ADM module (type=17) found
      this->gain.resize( adchans );
      this->offset.resize( adchans );

      // Check that the AD modules are installed in the correct slot
      //
      if ( ( type == 2 || type == 17 ) && ( module < 5 || module > 8 ) ) {
        message.str(""); message << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }

    } // end for ( auto line : lines )

    // empty the Archon log
    //
    error = this->fetchlog();

    // Make sure the following systemkeys are added.
    // They can be changed at any time by a command but since they have defaults
    // they don't require a command so this ensures they get into the systemkeys db.
    //
    std::stringstream keystr;
    keystr << "HDRSHIFT=" << this->n_hdrshift << "// number of HDR right-shift bits";
    this->systemkeys.addkey( keystr.str() );

    return(error);
  }
  /**************** Archon::Interface::connect_controller *********************/


  /***** Archon::Interface::disconnect_controller *****************************/
  /**
   * @brief      close connection to Archon and free allocated memory
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::disconnect_controller() {
    const std::string function("Archon::Interface::disconnect_controller");
    std::stringstream message;

    if (!this->archon.isconnected()) {
      logwrite(function, "connection already closed");
      return (NO_ERROR);
    }

    // close the socket file descriptor to the Archon controller
    //
    long error = this->archon.Close();

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      logwrite(function, "Archon connection terminated");
    }
    // Throw an error for any other errors
    //
    else {
      this->camera.log_error( function, "disconnecting Archon camera" );
    }

    return error;
  }
  /***** Archon::Interface::disconnect_controller *****************************/


  /***** Archon::Interface::cleanup_memory ************************************/
  /**
   * @brief      close connection to Archon and free allocated memory
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::cleanup_memory() {
    const std::string function("Archon::Interface::cleanup_memory");
    std::stringstream message;
    long error = NO_ERROR;

    // Free the memory
    //
    if ( this->image_data != nullptr ) {
      logwrite( function, "releasing allocated device memory" );
      delete [] this->image_data;
      this->image_data=nullptr;
    }

    // Free the image_ring buffers
    //
    message.str(""); message << "freed image ring buffer";
    for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
      if ( this->image_ring.at(i) != nullptr ) {
        delete [] this->image_ring.at(i);
        message << " " << std::dec << i << ":" << std::hex << (void*)this->image_ring.at(i);
        this->image_ring.at(i) = nullptr;
      }
    }
    logwrite( function, message.str() );

    // Free the work_ring buffers.
    // This takes a template function to typecast the pointer because it's defined as void.
    //
    {
    void* ptr=nullptr;
    switch ( this->camera_info.datatype ) {
      case USHORT_IMG: {
        this->free_workring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->free_workring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->free_workring( (uint32_t *)ptr );
        break;
      }
      case 0:   // not set
        break;
      default:
        message.str(""); message << "cannot free work_ring for unknown datatype: " << this->camera_info.datatype;
        this->camera.log_error(function, message.str());
        error = ERROR;
        break;
    }
    }

    // Free the cds ring buffers.
    // This takes a template function to typecast the pointer because it's defined as void.
    //
    {
    void* ptr=nullptr;
    switch ( this->cds_info.datatype ) {
      case USHORT_IMG: {
        this->free_cdsring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->free_cdsring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->free_cdsring( (uint32_t *)ptr );
        break;
      }
      case LONG_IMG: {
        this->free_cdsring( (int32_t *)ptr );
        break;
      }
      case 0:   // not set
        break;
      default:
        message.str(""); message << "cannot free cds_ring for unknown datatype: " << this->cds_info.datatype;
        this->camera.log_error(function, message.str());
        error = ERROR;
        break;
    }
    }

    return(error);
  }
  /***** Archon::Interface::cleanup_memory ************************************/


  /**************** Archon::Interface::native *********************************/
  /**
   * @fn     native
   * @brief  send native commands directly to Archon and log result
   * @param  std::string cmd
   * @return long ret from archon_cmd() call
   *
   * This function simply calls archon_cmd() then breaks the reply into
   * space-delimited tokens and puts each token into the asynchronous
   * message queue. The result is that the reply comes out one line at
   * a time on the async port.
   *
   */
  long Interface::native(std::string cmd) {
    const std::string function("Archon::Interface::native");
    std::stringstream message;
    std::string reply;
    std::vector<std::string> tokens;
    long ret = archon_cmd(cmd, reply);
    if (!reply.empty()) {
      // Tokenize the reply and put each non-empty token into the asynchronous message queue.
      // The reply message begins and ends with "CMD:BEGIN" and "CMD:END" and
      // each line of the reply is prepended with "CMD:" where CMD is the native command
      // which generated the message.
      //
      message << cmd << ":BEGIN";
      this->camera.async.enqueue( message.str() );

      Tokenize(reply, tokens, " ");
      for (long unsigned int tok=0; tok < tokens.size(); tok++) {
        if ( ! tokens[tok].empty() && tokens[tok] != "\n" ) {
          message.str(""); message << cmd << ":" << tokens[tok];
          this->camera.async.enqueue( message.str() );
        }
      }
      message.str(""); message << cmd << ":END";
      this->camera.async.enqueue( message.str() );
    }
    return( ret );
  }
  /**************** Archon::Interface::native *********************************/


  /**************** Archon::Interface::archon_cmd *****************************/
  /**
   * @fn     archon_cmd
   * @brief  send a command to Archon
   * @param  cmd
   * @param  reply (optional)
   * @return ERROR, BUSY or NO_ERROR
   *
   */
  long Interface::archon_cmd(std::string cmd) { // use this form when the calling
    std::string reply;                          // function doesn't need to look at the reply
    return( archon_cmd(cmd, reply) );
  }
  long Interface::archon_cmd(std::string cmd, std::string &reply) {
    const std::string function("Archon::Interface::archon_cmd");
    std::stringstream message;
    int     retval;
    char    check[4];
    int     error = NO_ERROR;

    // Nothing to do if no connection open to controller
    //
    if (!this->archon.isconnected()) {
      this->camera.log_error( function, "connection not open to controller" );
      return ERROR;
    }

    // Blocks to protect against simultaneous access, automatically
    // unlocks on return.
    //
    std::lock_guard<std::mutex> lock( this->archon_mutex );

    // The archon busy atomic flag is also needed because FETCH can keep
    // Archon busy for longer than the duration of this function.
    //
    if ( this->archon_busy.test_and_set() ) {
      message.str(""); message << "Archon busy: ignored command " << cmd;
      this->camera.log_error( function, message.str() );
      return BUSY;
    }

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    this->msgref = (this->msgref + 1) % 256;       // increment msgref for each new command sent
    std::stringstream ssprefix;
    ssprefix << ">"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << this->msgref;
    std::string prefix=ssprefix.str();

    std::transform( prefix.begin(), prefix.end(), prefix.begin(), ::toupper );    // make uppercase

    std::stringstream  sscmd;         // sscmd = stringstream, building command
    sscmd << prefix << cmd << "\n";
    std::string scmd = sscmd.str();   // scmd = string, command to send

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", this->msgref);

    // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
    //
    if ( (cmd.compare(0,7,"WCONFIG") != 0) &&
         (cmd.compare(0,5,"TIMER") != 0)   &&
         (cmd.compare(0,6,"STATUS") != 0)  &&
         (cmd.compare(0,5,"FRAME") != 0) ) {
      // erase newline for logging purposes
      std::string fcmd = scmd; try { fcmd.erase(fcmd.find("\n"), 1); } catch(...) { }
      message.str(""); message << "sending command: " << fcmd;
      logwrite(function, message.str());
    }

    // send the command
    //
    if ( (this->archon.Write(scmd)) == -1) {
      this->camera.log_error( function, "writing to camera socket");
    }

    // For the FETCH command we don't wait for a reply, but return immediately.
    // FETCH results in a binary response which is handled elsewhere (in read_frame).
    // Must also distinguish this from the FETCHLOG command, for which we do wait
    // for a normal reply.
    //
    // Do not clear the archon_busy flag because Archon is still busy!
    // The read_frame() function will have to clear this flag when it is
    // done reading the data.
    //
    if ( (cmd.compare(0,5,"FETCH")==0) && (cmd.compare(0,8,"FETCHLOG")!=0) ) return NO_ERROR;

    // For all other commands, receive the reply
    //
//  char    buffer[8192];                       // temporary buffer for holding Archon replies
    char* buffer = new char[8192]{};            // temporary buffer for holding Archon replies
    reply.clear();                              // zero reply buffer
    do {
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) { message.str(""); message << "Poll timeout waiting for response from Archon command (maybe unrecognized command?)"; error = TIMEOUT; }
        if (retval<0)  { message.str(""); message << "Poll error waiting for response from Archon command";   error = ERROR;   }
        if ( error != NO_ERROR ) this->camera.log_error( function, message.str() );
        break;
      }
      memset((void*)buffer, '\0', 8192);             // init temporary buffer
      retval = this->archon.Read(buffer, 8192);      // read into temp buffer
      if (retval <= 0) {
        this->camera.log_error( function, "reading Archon" );
        break; 
      }
      reply.append(buffer);                          // append read buffer into the reply string
    } while(retval>0 && reply.find("\n") == std::string::npos);

    delete [] buffer;

    // If there was an Archon error then clear the busy flag and get out now
    //
    if ( error != NO_ERROR ) {
      this->archon_busy.clear();
      return error;
    }

    // The first three bytes of the reply should contain the msgref of the
    // command, which can be used as a check that the received reply belongs
    // to the command which was sent.
    //
    // Error processing command (no other information is provided by Archon)
    //
    if (reply.compare(0, 1, "?")==0) {               // "?" means Archon experienced an error processing command
      error = ERROR;
      message.str(""); message << "Archon controller returned error processing command: " << cmd;
      this->camera.log_error( function, message.str() );
    }
    else
    if (reply.compare(0, 3, check)!=0) {             // First 3 bytes of reply must equal checksum else reply doesn't belong to command
      error = ERROR;
      std::string hdr = reply;
      try { scmd.erase(scmd.find("\n"), 1); } catch(...) { }
      message.str(""); message << "command-reply mismatch for command: " + scmd + ": expected " + check + " but received " + reply ;
      this->camera.log_error( function, message.str() );
    }
    else {                                           // command and reply are a matched pair
      error = NO_ERROR;

      // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
      if ( (cmd.compare(0,7,"WCONFIG") != 0) &&
           (cmd.compare(0,5,"TIMER") != 0)   &&
           (cmd.compare(0,6,"STATUS") != 0)  &&
           (cmd.compare(0,5,"FRAME") != 0) ) {
        message.str(""); message << "command 0x" << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << this->msgref << " success";
        logwrite(function, message.str());
      }

      reply.erase(0, 3);                             // strip off the msgref from the reply
    }

    // clear the busy flag
    //
    this->archon_busy.clear();

    return(error);
  }
  /**************** Archon::Interface::archon_cmd *****************************/


  /**************** Archon::Interface::read_parameter *************************/
  /**
   * @fn     read_parameter
   * @brief  read a parameter from Archon configuration memory
   * @param  paramname  char pointer to name of paramter
   * @param  value  reference to string for return value
   * @return ERROR on error, NO_ERROR if okay.
   *
   * The string reference contains the value of the parameter
   * to be returned to the user.
   *
   * No direct calls to Archon -- this function uses archon_cmd()
   * which in turn handles all of the Archon in-use locking.
   *
   */
  long Interface::read_parameter(std::string paramname, std::string &value) {
    const std::string function("Archon::Interface::read_parameter");
    std::stringstream message;
    std::stringstream cmd;
    std::string reply;
    long  error   = NO_ERROR;

    if (this->parammap.find(paramname.c_str()) == this->parammap.end()) {
      message.str(""); message << "parameter \"" << paramname << "\" not found in ACF";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // form the RCONFIG command to send to Archon
    //
    cmd.str("");
    cmd << "RCONFIG"
        << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
        << this->parammap[paramname.c_str()].line;
    error = this->archon_cmd(cmd.str(), reply);               // send RCONFIG command here

    if ( error != NO_ERROR ) {
      message.str(""); message << "ERROR: sending archon_cmd(" << cmd.str() << ")";
      logwrite( function, message.str() );
      return error;
    }

    try { reply.erase(reply.find("\n"), 1); } catch(...) { }  // strip newline

    // reply should now be of the form PARAMETERn=PARAMNAME=VALUE
    // and we want just the VALUE here
    //

    size_t loc;
    value = reply;
    if (value.compare(0, 9, "PARAMETER") == 0) {                                      // value: PARAMETERn=PARAMNAME=VALUE
      if ( (loc=value.find("=")) != std::string::npos ) value = value.substr(++loc);  // value: PARAMNAME=VALUE
      else {
        value="NaN";
        error = ERROR;
      }
      if ( (loc=value.find("=")) != std::string::npos ) value = value.substr(++loc);  // value: VALUE
      else {
        value="NaN";
        error = ERROR;
      }
    }
    else {
      value="NaN";
      error = ERROR;
    }

    if (error==ERROR) {
      message << "malformed reply: " << reply << " to Archon command " << cmd.str() << ": Expected PARAMETERn=PARAMNAME=VALUE";
      this->camera.log_error( function, message.str() );
    }
    else {
      message.str(""); message << paramname << " = " << value;
      logwrite(function, message.str());
    }
    return(error);
  }
  /**************** Archon::Interface::read_parameter *************************/


  /**************** Archon::Interface::prep_parameter *************************/
  /**
   * @fn     prep_parameter
   * @brief  
   * @param  
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   */
  long Interface::prep_parameter(std::string paramname, std::string value) {
    const std::string function("Archon::Interface::prep_parameter");
    std::stringstream message;
    std::stringstream scmd;
    long error = NO_ERROR;

    // Prepare to apply it to the system -- will be loaded on next EXTLOAD signal
    //
    scmd << "FASTPREPPARAM " << paramname << " " << value;
    error = this->archon_cmd(scmd.str());

    if (error != NO_ERROR) {
      message << "ERROR: prepping parameter \"" << paramname << "=" << value;
    }

    logwrite( function, message.str() );
    return(error);
  }
  /**************** Archon::Interface::prep_parameter *************************/


  /**************** Archon::Interface::load_parameter *************************/
  /**
   * @fn     load_parameter
   * @brief  
   * @param  
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   */
  long Interface::load_parameter(std::string paramname, std::string value) {
    const std::string function("Archon::Interface::load_parameter");
    std::stringstream message;
    std::stringstream scmd;
    long error = NO_ERROR;

    scmd << "FASTLOADPARAM " << paramname << " " << value;
    error = this->archon_cmd(scmd.str());

    if (error != NO_ERROR) {
      message << "ERROR: loading parameter \"" << paramname << "=" << value << "\" into Archon";
    }
    else {
      message << "parameter \"" << paramname << "=" << value << "\" loaded into Archon";
    }

    logwrite( function, message.str() );
    return(error);
  }
  /**************** Archon::Interface::load_parameter *************************/


  /**************** Archon::Interface::fetchlog *******************************/
  /**
   * @fn     fetchlog
   * @brief  fetch the archon log entry and log the response
   * @param  none
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   * Send the FETCHLOG command to, then read the reply from Archon.
   * Fetch until the log is empty. Log the response.
   *
   */
  long Interface::fetchlog() {
    const std::string function("Archon::Interface::fetchlog");
    std::string reply;
    std::stringstream message;
    long retval;

    // send FETCHLOG command while reply is not (null)
    //
    do {
      if ( (retval=this->archon_cmd(FETCHLOG, reply))!=NO_ERROR ) {          // send command here
        logwrite( function, "ERROR: calling FETCHLOG" );
        return(retval);
      }
      if (reply != "(null)") {
        try { reply.erase(reply.find("\n"), 1); } catch(...) { }             // strip newline
        logwrite(function, reply);                                           // log reply here
      }
    } while (reply != "(null)");                                             // stop when reply is (null)

    return(retval);
  }
  /**************** Archon::Interface::fetchlog *******************************/


  /**************** Archon::Interface::load_timing ****************************/
  /**
   * @fn     load_timing
   * @brief  loads the ACF file and applies the timing script and parameters only
   * @param  acffile, specified ACF to load
   * @param  retstring, reference to string for return values // TODO not yet implemented
   * @return 
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command 
   * which parses and compiles only the timing script and parameters.  
   *
   */
  long Interface::load_timing(std::string acffile, std::string &retstring) {
    return( this->load_timing( acffile ) );
  }
  long Interface::load_timing(std::string acffile) {
    const std::string function("Archon::Interface::load_timing");

    // load the ACF file and write to Archon configuration memory
    //
    long error = this->load_acf( acffile, true );

    // parse timing script and parameters and apply them to the system
    //
    if (error == NO_ERROR) error = this->archon_cmd(LOADTIMING);

    // defaults are required
    //
    if ( this->camera.default_roi.empty() )      { error=ERROR; logwrite( function, "ERROR missing default roi" ); }
    if ( this->camera.default_sampmode.empty() ) { error=ERROR; logwrite( function, "ERROR missing default sampmode" ); }
    if ( this->camera.default_exptime.empty() )  { error=ERROR; logwrite( function, "ERROR missing default exptime" ); }

    // exptime requires readout time,
    // and readout time requires ROI and sampmode,
    // so these should be set in this order the first time.
    //
    logwrite( function, "setting default ROI, sampmode, exptime" );
    if ( error==NO_ERROR ) error = this->region_of_interest( this->camera.default_roi );
    if ( error==NO_ERROR ) error = this->sample_mode( this->camera.default_sampmode );
    if ( error==NO_ERROR ) error = this->calc_readouttime();
    if ( error==NO_ERROR ) error = this->exptime( this->camera.default_exptime );

    return( error );
  }
  /**************** Archon::Interface::load_timing ****************************/


  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief  loads the ACF file and applies the complete system configuration
   * @param  none
   * @return 
   *
   * This function is overloaded.
   *
   * This version takes a single argument for the acf file to load.
   *
   * This function loads the ACF file and then sends an APPLYALL which
   * parses and applies the complete system configuration from the 
   * configuration memory just loaded. The detector power will be off.
   *
   */
  long Interface::load_firmware(std::string acffile) {
    const std::string function("Archon::Interface::load_firmware");
    // load the ACF file and write to Archon configuration memory
    //
    long error = this->load_acf( acffile, true );

    // Parse and apply the complete system configuration from configuration memory.
    // Detector power will be off after this.
    //
    if (error == NO_ERROR) error = this->archon_cmd(APPLYALL);

    if ( error != NO_ERROR ) this->fetchlog();

    // If no errors then automatically set the mode to DEFAULT.
    // This should come after APPLYALL in case any new parameters need to be written,
    // which shouldn't be done until after they have been applied.
    //
    if ( error == NO_ERROR ) error = this->set_camera_mode( std::string( "DEFAULT" ) );

    // defaults are required
    //
    if ( this->camera.default_roi.empty() )      { error=ERROR; logwrite( function, "ERROR missing default roi" ); }
    if ( this->camera.default_sampmode.empty() ) { error=ERROR; logwrite( function, "ERROR missing default sampmode" ); }
    if ( this->camera.default_exptime.empty() )  { error=ERROR; logwrite( function, "ERROR missing default exptime" ); }

    // exptime requires readout time,
    // and readout time requires ROI and sampmode,
    // so these should be set in this order the first time.
    //
    logwrite( function, "setting default ROI, sampmode, exptime" );
    if ( error==NO_ERROR ) error = this->region_of_interest( this->camera.default_roi );
    if ( error==NO_ERROR ) error = this->sample_mode( this->camera.default_sampmode );
    if ( error==NO_ERROR ) error = this->calc_readouttime();
    if ( error==NO_ERROR ) error = this->exptime( this->camera.default_exptime );

    return( error );
  }
  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief
   * @param  none
   * @return 
   *
   * This function is overloaded.
   *
   * This version is for future compatibility.
   * The multiple-controller version will pass a reference to a return string. //TODO
   *
   */
  long Interface::load_firmware(std::string acffile, std::string &retstring) {
    return( this->load_firmware( acffile ) );
  }
  /**************** Archon::Interface::load_firmware **************************/


  /**************** Archon::Interface::load_acf *******************************/
  /**
   * @brief      loads the ACF file into configuration memory (no APPLY!)
   * @param[in]  acffile          optional fully qualified path to ACF file
   * @param[in]  write_to_archon  true=write ACF to Archon, false=read into host memory
   * @return     ERROR or NO_ERROR
   *
   * This function loads the optionally specified file into configuration memory
   * (if not specified, then the file specified by DEFAULT_FIRMWARE will be used).
   * While the ACF is being read, an internal database (STL map) is being 
   * created to allow lookup access to the ACF file or parameters.
   *
   * The [MODE_XXX] sections are also parsed and parameters as a function
   * of mode are saved in their own database.
   *
   * This function only loads (WCONFIGxxx) the configuration memory; it does
   * not apply it to the system. Therefore, this function must be followed
   * with a LOADTIMING or APPLYALL command, for example.
   *
   * If write_to_archon is false then the Archon is not written to at all.
   * The ACF is read only into host memory. This is useful in case camerad
   * is stopped and restarted, it can use this to know about the previously
   * loaded ACF.
   *
   */
  long Interface::load_acf( std::string acffile, bool write_to_archon ) {
    const std::string function("Archon::Interface::load_acf");
    std::stringstream message;
    std::fstream filestream;  // I/O stream class
    std::string line;         // the line read from the acffile
    std::string mode;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::stringstream sscmd;
    std::string key, value;

    int      linecount;  // the Archon configuration line number is required for writing back to config memory
    long     error=NO_ERROR;
    bool     parse_config=false;

    // get the acf filename, either passed here or from loaded default
    //
    if ( acffile.empty() ) {
      message.str(""); message << "using DEFAULT_FIRMWARE from config file " << this->config.filename;
      logwrite( function, message.str() );
      acffile = this->camera.firmware[0];
    }
    else {
      this->camera.firmware[0] = acffile;
    }

    // try to open the file
    //
    try {
      filestream.open(acffile, std::ios_base::in);
    }
    catch(...) {
      message << "opening acf file " << acffile << ": " << std::strerror(errno);
      this->camera.log_error( function, message.str() );
      return ERROR;
    }
    if ( ! filestream.is_open() || ! filestream.good() ) {
      message << "acf file " << acffile << " could not be opened";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    logwrite(function, acffile);

    if ( write_to_archon ) {
      logwrite( function, "will write ACF to Archon" );
    }
    else {
      logwrite( function, "reading ACF into host memory only" );
    }

    // The CPU in Archon is single threaded, so it checks for a network 
    // command, then does some background polling (reading bias voltages etc), 
    // then checks again for a network command.  "POLLOFF" disables this 
    // background checking, so network command responses are very fast.  
    // The downside is that bias voltages, temperatures, etc are not updated 
    // until you give a "POLLON". 
    //
    if ( write_to_archon ) error = this->archon_cmd(POLLOFF);

    // clear configuration memory for this controller
    //
    if ( error == NO_ERROR && write_to_archon ) error = this->archon_cmd(CLEARCONFIG);

    if ( error != NO_ERROR  && write_to_archon ) { logwrite( function, "ERROR: could not prepare Archon for new ACF" ); return error; }

    // Any failure after clearing the configuration memory will mean
    // no firmware is loaded.
    //
    this->firmwareloaded = false;

    modemap.clear();                             // file is open, clear all modes

    linecount = 0;                               // init Archon configuration line number

    while ( getline(filestream, line) ) {        // note that getline discards the newline "\n" character

      // don't start parsing until [CONFIG] and stop on a newline or [SYSTEM]
      //
      if (line == "[CONFIG]") { parse_config=true;  continue; }
      if (line == "\n"      ) { parse_config=false; continue; }
      if (line == "[SYSTEM]") { parse_config=false; continue; }

      std::string savedline = line;              // save un-edited line for error reporting

      // parse mode sections, looking for "[MODE_xxxxx]"
      //
      if (line.substr(0,6)=="[MODE_") {          // this is a mode section
        try {
          line.erase(line.find("["), 1);         // erase the opening [ bracket
          line.erase(line.find("]"), 1);         // erase the closing ] bracket
        }
        catch(...) {
          message.str(""); message << "malformed mode section: " << savedline << ": expected [MODE_xxxx]";
          this->camera.log_error( function, message.str() );
	  filestream.close();
          return ERROR;
        }
        if ( ! line.empty() ) {                  // What's remaining should be MODE_xxxxx
          mode = line.substr(5);                 // everything after "MODE_" is the mode name
          std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

          // got a mode, now check if one of this name has already been located
          // and put into the modemap
          //
          if ( this->modemap.find(mode) != this->modemap.end() ) {
            message.str(""); message << "duplicate definition of mode: " << mode << ": load aborted";
            this->camera.log_error( function, message.str() );
	    filestream.close();
            return ERROR;
          }
          else {
            parse_config = true;
            message.str(""); message << "detected mode: " << mode; logwrite(function, message.str());
            this->modemap[mode].rawenable=-1;    // initialize to -1, require it be set somewhere in the ACF
                                                 // this also ensures something is saved in the modemap for this mode
          }
        }
        else {                                   // somehow there's no xxx left after removing "[MODE_" and "]"
          message.str(""); message << "malformed mode section: " << savedline << ": expected [MODE_xxxx]";
          this->camera.log_error( function, message.str() );
	  filestream.close();
          return ERROR;
        }
      }

      // Everything else is for parsing configuration lines so if we didn't get [CONFIG] then
      // skip to the next line.
      //
      if (!parse_config) continue;

      // replace any TAB characters with a space
      //
      string_replace_char(line, "\t", " ");

      // replace any backslash characters with a forward slash
      //
      string_replace_char(line, "\\", "/");

      // erase all quotes
      //
      try { line.erase( std::remove(line.begin(), line.end(), '"'), line.end() ); } catch(...) { }

      // Initialize key, value strings used to form WCONFIG KEY=VALUE command.
      // As long as key stays empty then the WCONFIG command is not written to the Archon.
      // This is what keeps TAGS: in the [MODE_xxxx] sections from being written to Archon,
      // because these do not populate key.
      //
      key.clear();
      value.clear();

      //  ************************************************************
      // Store actual Archon parameters in their own STL map IN ADDITION to the map
      // in which all other keywords are store, so that they can be accessed in
      // a different way.  Archon PARAMETER KEY=VALUE paris are formatted as:
      // PARAMETERn=ParameterName=value
      // where "PARAMETERn" is the key and "ParameterName=value" is the value.
      // However, it is logical to access them by ParameterName only. That is what the
      // parammap is for, hence the need for this STL map indexed on only the "ParameterName"
      // portion of the value. Conversely, the configmap is indexed by the key.
      // 
      // parammap stores ONLY the parameters, which are identified as PARAMETERxx="paramname=value"
      // configmap stores every configuration line sent to Archon (which includes parameters)
      //
      // In order to modify these keywords in Archon, the entire above phrase
      // (KEY=VALUE pair) must be preserved along with the line number on which it 
      // occurs in the config file.
      // ************************************************************

      // Look for TAGS: in the .acf file mode sections
      //
      // If tag is "ACF:" then it's a .acf line (could be a parameter or configuration)
      //
      if (line.compare(0,4,"ACF:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(4);                                            // strip off the "ACF:" portion

        try {
          Tokenize(line, tokens, "=");                                    // separate into tokens by "="

          if (tokens.size() == 1) {                                       // KEY=, the VALUE is empty
            key   = tokens[0];
            value = "";
          }
          else
          if (tokens.size() == 2) {                                       // KEY=VALUE
            key   = tokens[0];
            value = tokens[1];
          }
          else {
            message.str(""); message << "malformed ACF line: " << savedline << ": expected KEY=VALUE";
            this->camera.log_error( function, message.str() );
	    filestream.close();
            return ERROR;
          }

          bool keymatch = false;

          // If this key is in the main parammap then store it in the modemap's parammap for this mode
          //
          if (this->parammap.find( key ) != this->parammap.end()) {
            this->modemap[mode].parammap[ key ].name  = key;
            this->modemap[mode].parammap[ key ].value = value;
            keymatch = true;
          }

          // If this key is in the main configmap, then store it in the modemap's configmap for this mode
          //
          if (this->configmap.find( key ) != this->configmap.end()) {
            this->modemap[mode].configmap[ key ].value = value;
            keymatch = true;
          }

          // If this key is neither in the parammap nor in the configmap then return an error
          //
          if ( ! keymatch ) {
            message.str("");
            message << "[MODE_" << mode << "] ACF directive: " << key << "=" << value << " is not a valid parameter or configuration key";
            logwrite(function, message.str());
            filestream.close();
            return ERROR;
          }
        }
        catch ( ... ) {
          message.str(""); message << "extracting KEY=VALUE pair from ACF line: " << savedline;
          this->camera.log_error( function, message.str() );
          filestream.close();
          return ERROR;
        }
      } // end if (line.compare(0,4,"ACF:")==0)

      // The "ARCH:" tag is for internal (Archon_interface) variables
      // using the KEY=VALUE format.
      //
      else
      if (line.compare(0,5,"ARCH:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(5);                                            // strip off the "ARCH:" portion
        Tokenize(line, tokens, "=");                                      // separate into KEY, VALUE tokens
        if (tokens.size() != 2) {
          message.str(""); message << "malformed ARCH line: " << savedline << ": expected ARCH:KEY=VALUE";
          this->camera.log_error( function, message.str() );
	  filestream.close();
          return ERROR;
        }

        int ivalue = std::stoi( tokens[1] );
        if ( ivalue < 0 ) {
          message.str(""); message << "ERROR value for " << tokens[0] << " cannot be negative";
          this->camera.log_error( function, message.str() );
	  filestream.close();
          return ERROR;
        }
        if ( tokens[0] == "NUM_DETECT" ) {
          this->modemap[mode].geometry.num_detect = ivalue;
        }
        else
        if ( tokens[0] == "HORI_AMPS" ) {
          this->modemap[mode].geometry.amps[0] = ivalue;
        }
        else
        if ( tokens[0] == "VERT_AMPS" ) {
          this->modemap[mode].geometry.amps[1] = ivalue;
        }
        else {
          message.str(""); message << "unrecognized internal parameter specified: "<< tokens[0];
          this->camera.log_error( function, message.str() );
	  filestream.close();
          return(ERROR);
        }
      } // end if (line.compare(0,5,"ARCH:")==0)

      // the "FITS:" tag is used to write custom keyword entries of the form "FITS:KEYWORD=VALUE/COMMENT"
      //
      else
      if (line.compare(0,5,"FITS:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(5);                                            // strip off the "FITS:" portion

        // First, tokenize on the equal sign "=".
        // The token left of "=" is the keyword. Immediate right is the value
        Tokenize(line, tokens, "=");
        if (tokens.size() != 2) {                                         // need at least two tokens at this point
          message.str(""); message << "malformed FITS command: " << savedline << ": expected KEYWORD=value/comment";
          this->camera.log_error( function, message.str() );
          filestream.close();
          return(ERROR);
        }
        keyword   = tokens[0].substr(0,8);                                // truncate keyword to 8 characters
        keystring = tokens[1];                                            // tokenize the rest in a moment
        keycomment = "";                                                  // initialize comment, assumed empty unless specified below

        // Next, tokenize on the slash "/".
        // The token left of "/" is the value. Anything to the right is a comment.
        //
        Tokenize(keystring, tokens, "/");

        if (tokens.size() == 0) {      // no tokens found means no "/" characeter which means no comment
          keyvalue = keystring;        // therefore the keyvalue is the entire string
        }

        if (tokens.size() > 0) {       // at least one token
          keyvalue = tokens[0];
        }

        if (tokens.size() == 2) {      // If there are two tokens here then the second is a comment
          keycomment = tokens[1];
        }

        if (tokens.size() > 2) {       // everything below this has been covered
          message.str(""); message << "malformed FITS command: " << savedline << ": expected KEYWORD=VALUE/COMMENT";
          this->camera.log_error( function, message.str() );
          message.str(""); message << "too many \"/\" in comment string? " << keystring;
          this->camera.log_error( function, message.str() );
          filestream.close();
          return(ERROR);
        }

        // Save all of the user keyword information in a map for later
        //
        this->modemap[mode].acfkeys.keydb[keyword].keyword    = keyword;
        this->modemap[mode].acfkeys.keydb[keyword].keytype    = this->camera_info.userkeys.get_keytype(keyvalue);
        this->modemap[mode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
        this->modemap[mode].acfkeys.keydb[keyword].keycomment = keycomment;
      } // end if (line.compare(0,5,"FITS:")==0)

      //
      // ----- all done looking for "TAGS:" -----
      //

      // If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...
      //
      else
      if ( (line.compare(0,11,"PARAMETERS=")!=0) &&   // not the "PARAMETERS=xx line
           (line.compare(0, 9,"PARAMETER"  )==0) ) {  // but must start with "PARAMETER"

        std::vector<std::string> tokens;
        Tokenize(line, tokens, "=");                  // separate into PARAMETERn, ParameterName, value tokens

        if (tokens.size() != 3) {
          message.str(""); message << "malformed paramter line: " << savedline << ": expected PARAMETERn=Param=value";;
          this->camera.log_error( function, message.str() );
          filestream.close();
          return ERROR;
        }

        // Tokenize broke everything up at the "=" and
        // we need all three parts but we also need a version containing the last
        // two parts together, "ParameterName=value" so re-assemble them here.
        //
        std::stringstream paramnamevalue;
        paramnamevalue << tokens[1] << "=" << tokens[2];             // reassemble ParameterName=value string

        // build an STL map "configmap" indexed on PARAMETERn, the part before the first "=" sign
        //
        this->configmap[ tokens[0] ].line  = linecount;              // configuration line number
        this->configmap[ tokens[0] ].value = paramnamevalue.str();   // configuration value for PARAMETERn

        // build an STL map "parammap" indexed on ParameterName so that we can lookup by the actual name
        //
        this->parammap[ tokens[1] ].key   = tokens[0];          // PARAMETERn
        this->parammap[ tokens[1] ].name  = tokens[1] ;         // ParameterName
        this->parammap[ tokens[1] ].value = tokens[2];          // value
        this->parammap[ tokens[1] ].line  = linecount;          // line number

        // assemble a KEY=VALUE pair used to form the WCONFIG command
        key   = tokens[0];                                      // PARAMETERn
        value = paramnamevalue.str();                           // ParameterName=value
      } // end If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...

      // ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
      // to be indexed by the key. Some lines may be equal to blank, e.g. "CONSTANTx=" so that
      // only one token is made
      //
      else {
        std::vector<std::string> tokens;
        // Tokenize will return a size=1 even if there are no delimiters,
        // so work around this by first checking for delimiters
        // before calling Tokenize.
        //
        if (line.find_first_of("=", 0) == std::string::npos) {
          continue;
        }
        Tokenize(line, tokens, "=");                            // separate into KEY, VALUE tokens
        if (tokens.size() == 0) {
          continue;                                             // nothing to do here if no tokens (ie no "=")
        }
        if (tokens.size() > 0 ) {                               // at least one token is the key
          key   = tokens[0];                                    // KEY
          value = "";                                           // VALUE can be empty (e.g. labels not required)
          this->configmap[ tokens[0] ].line  = linecount;
          this->configmap[ tokens[0] ].value = value;     
        }
        if (tokens.size() > 1 ) {                               // if a second token then that's the value
          value = tokens[1];                                    // VALUE (there is a second token)
          this->configmap[ tokens[0] ].value = tokens[1];
        }
      } // end else

      // Form the WCONFIG command to Archon and
      // write the config line to the controller memory (if key is not empty).
      //
      if ( !key.empty() ) {                                     // value can be empty but key cannot
        sscmd.str("");
        sscmd << "WCONFIG"
              << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
              << linecount
              << key << "=" << value << "\n";
        // send the WCONFIG command here
        if ( error == NO_ERROR && write_to_archon ) error = this->archon_cmd(sscmd.str());
      } // end if ( !key.empty() && !value.empty() )
      linecount++;
    } // end while ( getline(filestream, line) )

    // re-enable background polling
    //
    if ( error == NO_ERROR && write_to_archon ) error = this->archon_cmd(POLLON);

    filestream.close();
    if (error == NO_ERROR) {
      logwrite(function, "loaded Archon config file OK");
      this->firmwareloaded = true;

      // add firmware filename and checksum to systemkeys keyword database
      //
      this->systemkeys.addkey( "FIRMWARE=" + acffile + "// controller firmware" );

      std::string hash;
      md5_file( acffile, hash );
      this->systemkeys.addkey( "FIRM_MD5=" + hash + "// MD5 checksum of firmware" );
    }

    // If there was an Archon error then read the Archon error log
    //
    if (error != NO_ERROR) error = this->fetchlog();

    this->modeselected = false;           // require that a mode be selected after loading new firmware

    // Even if exptime, longexposure were previously set, a new ACF could have different
    // default values than the server has, so reset these to "undefined" in order to
    // force the server to ask for them.
    //
    this->camera_info.exposure_time   = -1;
    this->camera_info.exposure_factor = -1;
    this->camera_info.exposure_unit.clear();

    // clear the sampmode to force setting it after reloading
    //
    this->camera_info.sampmode = -1;

    return(error);
  }
  /**************** Archon::Interface::load_acf *******************************/


  /**************** Archon::Interface::set_camera_mode ************************/
  /**
   * @fn     set_camera_mode
   * @brief  
   * @param  none
   * @return 
   *
   */
  long Interface::set_camera_mode(std::string mode) {
    const std::string function("Archon::Interface::set_camera_mode");
    std::stringstream message;
    bool configchanged = false;
    bool paramchanged = false;
    long error;

    // Cannot change while exposure in progress
    //
    if ( this->camera.is_exposing() ) {
      this->camera.log_error( function, "cannot change camera mode while exposure in progress" );
      return(ERROR);
    }

    // No point in trying anything if no firmware has been loaded yet
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "no firmware loaded" );
      return(ERROR);
    }

    std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

    // The requested mode must have been read in the current ACF file
    // and put into the modemap...
    //
    if (this->modemap.find(mode) == this->modemap.end()) {
      message.str(""); message << "undefined mode " << mode << " in ACF file " << this->camera.firmware[0];
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // load specific mode settings from .acf and apply to Archon
    //
    if ( load_mode_settings(mode) != NO_ERROR) {
      message.str(""); message << "ERROR: failed to load mode settings for mode: " << mode;
      logwrite( function, message.str() );
      return(ERROR);
    }

    // set internal variables based on new .acf values loaded
    //
    error = NO_ERROR;
    if (error==NO_ERROR) error = get_configmap_value("FRAMEMODE", this->modemap[mode].geometry.framemode);
    if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->modemap[mode].geometry.linecount);
    if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->modemap[mode].geometry.pixelcount);
    if (error==NO_ERROR) error = get_configmap_value("RAWENABLE", this->modemap[mode].rawenable);
    if (error==NO_ERROR) error = get_configmap_value("RAWSEL", this->rawinfo.adchan);
    if (error==NO_ERROR) error = get_configmap_value("RAWSAMPLES", this->rawinfo.rawsamples);
    if (error==NO_ERROR) error = get_configmap_value("RAWENDLINE", this->rawinfo.rawlines);
#ifdef LOGLEVEL_DEBUG
    message.str(""); 
    message << "[DEBUG] mode=" << mode << " RAWENABLE=" << this->modemap[mode].rawenable 
            << " RAWSAMPLES=" << this->rawinfo.rawsamples << " RAWLINES=" << this->rawinfo.rawlines;
    logwrite(function, message.str());
#endif

    // get out if any errors at this point
    //
    if ( error != NO_ERROR ) { logwrite( function, "ERROR: one or more internal variables missing from configmap" ); return error; }

    int num_detect = this->modemap[mode].geometry.num_detect;             // for convenience

    // set current number of Archon buffers and resize local memory
    // get out if an error
    //
    int bigbuf=-1;
    if (error==NO_ERROR) error = get_configmap_value("BIGBUF", bigbuf);   // get value of BIGBUF from loaded acf file
    this->camera_info.activebufs = (bigbuf==1) ? 2 : 3;                   // set number of active buffers based on BIGBUF
    if ( error != NO_ERROR ) { logwrite( function, "ERROR: unable to read BIGBUF from ACF" ); return error; }

    // There is one special reserved mode name, "RAW"
    //
    if (mode=="RAW") {
      this->camera_info.detector_pixels[0] = this->rawinfo.rawsamples;
      this->camera_info.detector_pixels[1] = this->rawinfo.rawlines; 
      this->camera_info.detector_pixels[1]++;
      // frame_type will determine the bits per pixel and where the detector_axes come from
      this->camera_info.frame_type = Camera::FRAME_RAW;
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (RAWSAMPLES) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (RAWENDLINE) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
#endif
    }

    // Any other mode falls under here
    //
    else {
      if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->camera_info.detector_pixels[0]);
      if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->camera_info.detector_pixels[1]);
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] mode=" << mode; logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0]
                               << " amps[0] = " << this->modemap[mode].geometry.amps[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1]
                               << " amps[1] = " << this->modemap[mode].geometry.amps[1];
      logwrite(function, message.str());
#endif
      this->camera_info.detector_pixels[0] *= this->modemap[mode].geometry.amps[0];
      this->camera_info.detector_pixels[1] *= this->modemap[mode].geometry.amps[1];
      this->camera_info.frame_type = Camera::FRAME_IMAGE;
      // ROI is the full detector
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
#endif
      if ( error != NO_ERROR ) { logwrite( function, "ERROR: unable to get PIXELCOUNT,LINECOUNT from ACF" ); return error; }
    }

    // set bitpix based on SAMPLEMODE
    //
    int samplemode=-1;
    if (error==NO_ERROR) error = get_configmap_value("SAMPLEMODE", samplemode); // SAMPLEMODE=0 for 16bpp, =1 for 32bpp
    if ( error != NO_ERROR ) { logwrite( function, "ERROR: unable to get SAMPLEMODE from ACF" ); return error; }
    if (samplemode < 0) { this->camera.log_error( function, "bad or missing SAMPLEMODE from ACF" ); return ERROR; }
    this->camera_info.bitpix = (samplemode==0) ? 16 : 32;

    // Load parameters and Apply CDS/Deint configuration if any of them changed
    //
    if ((error == NO_ERROR) && paramchanged)  error = this->archon_cmd(LOADPARAMS);  // TODO I think paramchanged is never set!
    if ((error == NO_ERROR) && configchanged) error = this->archon_cmd(APPLYCDS);    // TODO I think configchaned is never set!

    // Get the current frame buffer status
    if (error == NO_ERROR) error = this->get_frame_status();
    if (error != NO_ERROR) {
      logwrite( function, "ERROR: unable to get frame status" );
      return(error);
    }

    // Set axes, image dimensions, calculate image_memory, etc.
    // Raw will always be 16 bpp (USHORT).
    // Image can be 16 or 32 bpp depending on SAMPLEMODE setting in ACF.
    // Call set_axes(datatype) with the FITS data type needed, which will set the info.datatype variable.
    //
    error = this->camera_info.set_axes();                                                 // 16 bit raw is unsigned short int
/*********
    if (this->camera_info.frame_type == Camera::FRAME_RAW) {
      error = this->camera_info.set_axes(USHORT_IMG);                                     // 16 bit raw is unsigned short int
    }
    if (this->camera_info.frame_type == Camera::FRAME_IMAGE) {
      if (this->camera_info.bitpix == 16) error = this->camera_info.set_axes(SHORT_IMG);  // 16 bit image is short int
      else
      if (this->camera_info.bitpix == 32) error = this->camera_info.set_axes(FLOAT_IMG);  // 32 bit image is float
      else {
        message.str(""); message << "bad bitpix " << this->camera_info.bitpix << ": expected 16 | 32";
        this->camera.log_error( function, message.str() );
        return (ERROR);
      }
    }
*********/
    if (error != NO_ERROR) {
      this->camera.log_error( function, "setting axes" );
      return (ERROR);
    }

    // Allocate image_data in blocks because the controller outputs data in units of blocks.
    // If there are multiple slices (cubes) then this is the memory per slice, just one image read.
    //
    this->image_data_bytes = (uint32_t) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN ) * BLOCK_LEN;

    if (this->image_data_bytes == 0) {
      this->camera.log_error( function, "image data size is zero! check NUM_DETECT, HORI_AMPS, VERT_AMPS in .acf file" );
      error = ERROR;
    }

    this->camera_info.current_observing_mode = mode;       // identify the newly selected mode in the camera_info class object
    this->modeselected = true;                             // a valid mode has been selected

    message.str(""); message << "new mode: " << mode << " will use " << this->camera_info.bitpix << " bits per pixel";
    logwrite(function, message.str());

    // Calculate amplifier sections
    //
    int rows = this->modemap[mode].geometry.linecount;     // rows per section
    int cols = this->modemap[mode].geometry.pixelcount;    // cols per section

    int hamps = this->modemap[mode].geometry.amps[0];      // horizontal amplifiers
    int vamps = this->modemap[mode].geometry.amps[1];      // vertical amplifiers

    int x0=-1, x1, y0, y1;                                 // for indexing
    std::vector<long> coords;                              // vector of coordinates, convention is x0,x1,y0,y1
    int framemode = this->modemap[mode].geometry.framemode;

    this->camera_info.amp_section.clear();                 // vector of coords vectors, one set of coords per extension

    for ( int y=0; y<vamps; y++ ) {
      for ( int x=0; x<hamps; x++ ) {
        if ( framemode == 2 ) {
          x0 = x; x1=x+1;
          y0 = y; y1=y+1;
        }
        else {
          x0++;   x1=x0+1;
          y0 = 0; y1=1;
        }
        coords.clear();
        coords.push_back( (x0*cols + 1) );                 // x0 is xstart
        coords.push_back( (x1)*cols );                     // x1 is xstop, xrange = x0:x1
        coords.push_back( (y0*rows + 1) );                 // y0 is ystart
        coords.push_back( (y1)*rows );                     // y1 is ystop, yrange = y0:y1

        this->camera_info.amp_section.push_back( coords ); // (x0,x1,y0,y1) for this extension

      }
    }
    message.str(""); message << "identified " << this->camera_info.amp_section.size() << " amplifier sections";
    logwrite( function, message.str() );

#ifdef LOGLEVEL_DEBUG
    int ext=0;
    for ( auto sec : this->camera_info.amp_section ) {
      message.str(""); message << "[DEBUG] extension " << ext++ << ":";
      for ( auto xy : sec ) {
        message << " " << xy;
      }
      logwrite( function, message.str() );
    }
#endif

    return(error);
  }
  /**************** Archon::Interface::set_camera_mode ************************/


  /**************** Archon::Interface::load_mode_settings *********************/
  /**
   * @fn     load_mode_settings
   * @brief  load into Archon settings specified in mode section of .acf file
   * @param  camera mode
   * @return none
   *
   * At the end of the .acf file are optional sections for each camera
   * observing mode. These sections can contain any number of configuration
   * lines and parameters to set for the given mode. Those lines are read
   * when the configuration file is loaded. This function writes them to
   * the Archon controller.
   */
  long Interface::load_mode_settings(std::string mode) {
    const std::string function("Archon::Interface::load_mode_settings");
    std::stringstream message;

    long error=NO_ERROR;
    cfg_map_t::iterator   cfg_it;
    param_map_t::iterator param_it;
                     bool paramchanged  = false;
                     bool configchanged = false;

    std::stringstream errstr;

    /**
     * iterate through configmap, writing each config key in the map
     */
    for (cfg_it  = this->modemap[mode].configmap.begin();
         cfg_it != this->modemap[mode].configmap.end();
         cfg_it++) {
      error = this->write_config_key( cfg_it->first.c_str(), cfg_it->second.value.c_str(), configchanged );
      if (error != NO_ERROR) {
        errstr  << "ERROR: writing config key:" << cfg_it->first << " value:" << cfg_it->second.value << " for mode " << mode;
        break;
      }
    }

    /**
     * if no errors from writing config keys, then
     * iterate through the parammap, writing each parameter in the map
     */
    if (error == NO_ERROR) {
      for (param_it  = this->modemap[mode].parammap.begin();
           param_it != this->modemap[mode].parammap.end();
           param_it++) {
        error = this->write_parameter( param_it->first.c_str(), param_it->second.value.c_str(), paramchanged );
        message.str(""); message << "paramchanged=" << (paramchanged?"true":"false");
        logwrite(function, message.str());
        if (error != NO_ERROR) {
          errstr  << "ERROR: writing parameter key:" << param_it->first << " value:" << param_it->second.value << " for mode " << mode;
          break;
        }
      }
    }

    /**
     * apply the new settings to the system here, only if something changed
     */
    if ( (error == NO_ERROR) && paramchanged  ) error = this->archon_cmd(LOADPARAMS);
    if ( (error == NO_ERROR) && configchanged ) error = this->archon_cmd(APPLYCDS);

    if (error == NO_ERROR) {
      message.str(""); message << "loaded mode: " << mode;
      logwrite(function, message.str());
    }
    else {
      logwrite( function, errstr.str() );
      return error;
    }

    // The new mode could contain a ShutterEnable param,
    // and if it does then the server needs to know about that.
    //
    if ( !this->shutenableparam.empty() ) {

      // first read the parameter
      //
      std::string lshutten;
      if ( read_parameter( this->shutenableparam, lshutten ) != NO_ERROR ) { 
        message.str(""); message << "ERROR: reading \"" << this->shutenableparam << "\" parameter from Archon";
        logwrite( function, message.str() );
        return ERROR;
      }

      // parse the parameter value
      // and convert it to a string for the shutter command
      //
      std::string shuttenstr;
      if ( lshutten == "1" ) shuttenstr = "enable";
      else
      if ( lshutten == "0" ) shuttenstr = "disable";
      else {
        message.str(""); message << "ERROR: unrecognized shutter enable parameter value " << lshutten << ": expected {0,1}";
        logwrite( function, message.str() );
        return ERROR;
      }

      // Tell the server
      //
      std::string dontcare;
      if ( this->shutter( shuttenstr, dontcare ) != NO_ERROR ) { logwrite( function, "ERROR: setting shutter enable parameter" ); return ERROR; }
    }

    /**
     * read back some TAPLINE information
     */
    if (error==NO_ERROR) error = get_configmap_value("TAPLINES", this->taplines); // total number of taps

    std::vector<std::string> tokens;
    std::stringstream        tap;
    std::string              adchan;

    // Remove all GAIN* and OFFSET* keywords from the systemkeys database
    // because the new mode could have different channels (so simply 
    // over-writing existing keys might not be sufficient). 
    //
    // The new GAIN* and OFFSET* system keys will be added in the next loop.
    //
    this->systemkeys.EraseKeys( "GAIN" );
    this->systemkeys.EraseKeys( "OFFSET" );

    // Loop through every tap to get the offset for each
    //
    for (int tapn=0; tapn<this->taplines; tapn++) {
      tap.str("");
      tap << "TAPLINE" << tapn;  // used to find the tapline in the configmap

      // The value of TAPLINEn = ADxx,gain,offset --
      // tokenize by comma to separate out each parameter...
      //
      Tokenize(this->configmap[tap.str().c_str()].value, tokens, ",");

      // If all three tokens present (A?xx,gain,offset) then parse it,
      // otherwise it's an unused tap and we can skip it.
      //
      if (tokens.size() == 3) { // defined tap has three tokens
        adchan = tokens[0];     // AD channel is the first (0th) token
        char chars[] = "ADMLR"; // characters to remove in order to get just the AD channel number

        // Before removing these characters, set the max allowed AD number based on the tapline syntax.
        // "ADxx,gain,offset" is for ADC module
        // "AMxx,gain,offset" is for ADM module
        //
        int admax = 0;
        if ( adchan.find("AD") != std::string::npos ) admax = MAXADCCHANS;
        else
        if ( adchan.find("AM") != std::string::npos ) admax = MAXADMCHANS;
        else {
          message.str(""); message << "bad tapline syntax. Expected ADn or AMn but got " << adchan;
          this->camera.log_error( function, message.str() );
          return( ERROR );
        }

        // remove AD, AM, L, R from the adchan string, to get just the AD channel number
        //
        for (unsigned int j = 0; j < strlen(chars); j++) {
          adchan.erase(std::remove(adchan.begin(), adchan.end(), chars[j]), adchan.end());
        }

        // AD# in TAPLINE is 1-based (numbered 1,2,3,...)
        // but convert here to 0-based (numbered 0,1,2,...) and check value before using
        //
        int adnum;
        try {
          adnum = std::stoi(adchan) - 1;
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert AD number \'" << adchan << "\' to integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          this->camera.log_error( function, "AD number out of integer range" );
          return(ERROR);
        }
        if ( (adnum < 0) || (adnum > admax) ) {
          message.str(""); message << "ADC channel " << adnum << " outside range {0:" << admax << "}";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        // Now that adnum is OK, convert next two tokens to gain, offset
        //
        int gain_try=0, offset_try=0;
        try {
          gain_try   = std::stoi( tokens[1] );      // gain as function of AD channel
          offset_try = std::stoi( tokens[2] );      // offset as function of AD channel
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert GAIN \"" << tokens[1] << "\" and/or OFFSET \"" << tokens[2] << "\" to integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "GAIN " << tokens[1] << ", OFFSET " << tokens[2] << " outside integer range";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        // Now assign the gain,offsets to their appropriate position in the vectors
        //
        try {
          this->gain.at( adnum )   = gain_try;      // gain as function of AD channel
          this->offset.at( adnum ) = offset_try;    // offset as function of AD channel

          // Add the gain/offset as system header keywords if configured to do so.
          // "write_tapinfo_to_fits" is default true but can be overridden in the config file.
          //
          std::stringstream keystr;
          keystr.str(""); keystr << "GAIN" << std::setfill('0') << std::setw(2) << adnum << "=" << this->gain.at( adnum )
                                 << "// gain for AD chan " << adnum;
          if ( this->write_tapinfo_to_fits ) this->systemkeys.addkey( keystr.str() );
          keystr.str(""); keystr << "OFFSET" << std::setfill('0') << std::setw(2) << adnum << "=" << this->offset.at( adnum )
                                 << "// offset for AD chan " << adnum;
          if ( this->write_tapinfo_to_fits ) this->systemkeys.addkey( keystr.str() );
        }
        catch ( std::out_of_range & ) {
          message.str(""); message << "AD# " << adnum << " outside range {0:" << (this->gain.size() & this->offset.size()) << "}";
          this->camera.log_error( function, message.str() );
          if ( this->gain.size()==0 || this->offset.size()==0 ) {
            this->camera.log_error( function, "gain/offset vectors are empty: no ADC or ADM board installed?" );
          }
          return( ERROR );
        }
      }
    }

    return(error);
  }
  /**************** Archon::Interface::load_mode_settings *********************/


  /**************** Archon::Interface::get_frame_status ***********************/
  /**
   * @fn     get_frame_status
   * @brief  get the current frame buffer status
   * @param  none
   * @return 
   *
   * Sends the "FRAME" command to Archon, reads back the reply, then parses the
   * reply and stores parameters into the framestatus structure 
   * (of type frame_data_t).
   *
   */
  long Interface::get_frame_status() {
    const std::string function("Archon::Interface::get_frame_status");
    std::string reply;
    std::stringstream message;
    int   newestframe, newestbuf;
    long  error=NO_ERROR;
//logwrite( function, "[TIMESTAMP] start" );

    // send FRAME command to get frame buffer status
    //
    if ( (error = this->archon_cmd(FRAME, reply)) ) {
      if ( error == ERROR ) logwrite( function, "ERROR: sending FRAME command" );  // don't log here if BUSY
      return(error);
    }

    // First Tokenize breaks the single, continuous string into vector of individual strings,
    // from "TIMER=xxxx RBUF=xxxx " to:
    //   tokens[0] : TIMER=xxxx
    //   tokens[1] : RBUF=xxxx
    //   tokens[2] : etc.
    //
    std::vector<std::string> tokens;
    Tokenize(reply, tokens, " ");

    for (size_t i=0; i<tokens.size(); i++) {

      // Second Tokenize separates the paramater from the value
      //
      std::vector<std::string> subtokens;
//    subtokens.clear();
      Tokenize(tokens[i], subtokens, "=");

      // Each entry in the FRAME message must have two tokens, one for each side of the "=" equal sign
      // (in other words there must be two subtokens per token)
      //
      if (subtokens.size() != 2) {
        message.str("");
        message << "expected 2 but received invalid number of tokens (" << subtokens.size() << ") in FRAME message:";
        for (size_t j=0; i<subtokens.size(); i++) message << " " << subtokens.at(j);
        this->camera.log_error( function, message.str() );
        return(ERROR);  // We could continue; but if one is bad then we could miss seeing a larger problem
      }

      int bufnum=0;
      int value=0;
      uint64_t lvalue=0;

      try {
        if (subtokens.at(0)=="TIMER") this->frame.timer = subtokens.at(1);  // timer is a string
        else {                                                        // everything else is going to be a number
          if (subtokens.at(0).compare(0, 3, "BUF")==0) {               // for all "BUFnSOMETHING=VALUE" we want the bufnum "n"
            bufnum = std::stoi( subtokens.at(0).substr(3, 1) );        // extract the "n" here which is 1-based (1,2,3)
          }
          if (subtokens.at(0).substr(4)=="BASE" ) {                    // for "BUFnBASE=xxx" the value is uint64
            lvalue  = std::stol( subtokens.at(1) );                    // this value will get assigned to the corresponding parameter
          }
          else
          if (subtokens.at(0).find("TIMESTAMP")!=std::string::npos) {  // for any "xxxTIMESTAMPxxx" the value is uint64
            std::stringstream v;
            v << std::hex << subtokens.at(1);
            v >> lvalue;                                            // this value will get assigned to the corresponding parameter
          }
          else                                                      // everything else is an int
            value  = std::stoi( subtokens.at(1) );                     // this value will get assigned to the corresponding parameter
        }
        if (subtokens.at(0)=="RBUF")  this->frame.rbuf  = value;
        if (subtokens.at(0)=="WBUF")  this->frame.wbuf  = value;
      }
      catch (std::invalid_argument &) {
        this->camera.log_error( function, "unable to convert buffer or value from FRAME message to integer" );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        this->camera.log_error( function, "buffer or value from FRAME message outside integer range" );
        return(ERROR);
      }

      // The next group are BUFnSOMETHING=VALUE
      // Extract the "n" which must be a number from 1 to Archon::nbufs
      // After getting the buffer number we assign the corresponding value.
      //
      if (subtokens.at(0).compare(0, 3, "BUF")==0) {
        if (bufnum < 1 || bufnum > Archon::nbufs) {
          message.str(""); message << "buffer number " << bufnum << " from FRAME message outside range {1:" << Archon::nbufs << "}";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        bufnum--;   // subtract 1 because it is 1-based in the message but need 0-based for the indexing
        if (subtokens.at(0).substr(4) == "SAMPLE")      this->frame.bufsample[bufnum]      =  value;
        if (subtokens.at(0).substr(4) == "COMPLETE")    this->frame.bufcomplete[bufnum]    =  value;
        if (subtokens.at(0).substr(4) == "MODE")        this->frame.bufmode[bufnum]        =  value;
        if (subtokens.at(0).substr(4) == "BASE")        this->frame.bufbase[bufnum]        = lvalue;
        if (subtokens.at(0).substr(4) == "FRAME")       this->frame.bufframen[bufnum]      =  value;
        if (subtokens.at(0).substr(4) == "WIDTH")       this->frame.bufwidth[bufnum]       =  value;
        if (subtokens.at(0).substr(4) == "HEIGHT")      this->frame.bufheight[bufnum]      =  value;
        if (subtokens.at(0).substr(4) == "PIXELS")      this->frame.bufpixels[bufnum]      =  value;
        if (subtokens.at(0).substr(4) == "LINES")       this->frame.buflines[bufnum]       =  value;
        if (subtokens.at(0).substr(4) == "RAWBLOCKS")   this->frame.bufrawblocks[bufnum]   =  value;
        if (subtokens.at(0).substr(4) == "RAWLINES")    this->frame.bufrawlines[bufnum]    =  value;
        if (subtokens.at(0).substr(4) == "RAWOFFSET")   this->frame.bufrawoffset[bufnum]   =  value;
        if (subtokens.at(0).substr(4) == "TIMESTAMP")   this->frame.buftimestamp[bufnum]   = lvalue;
        if (subtokens.at(0).substr(4) == "RETIMESTAMP") this->frame.bufretimestamp[bufnum] = lvalue;
        if (subtokens.at(0).substr(4) == "FETIMESTAMP") this->frame.buffetimestamp[bufnum] = lvalue;
      }
    }

    newestbuf   = this->frame.index;

    if (this->frame.index < (int)this->frame.bufframen.size()) {
      newestframe = this->frame.bufframen[this->frame.index];
    }
    else {
      message.str(""); message << "newest buf " << this->frame.index << " from FRAME message exceeds number of buffers " << this->frame.bufframen.size();
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // loop through the number of buffers
    //
    int num_zero = 0;
    for (int bc=0; bc<Archon::nbufs; bc++) {

      // look for special start-up case, when all frame buffers are zero
      //
      if ( this->frame.bufframen[bc] == 0 ) num_zero++;

      if ( (this->frame.bufframen[bc] > newestframe) &&
            this->frame.bufcomplete[bc] ) {
        newestframe = this->frame.bufframen[bc];
        newestbuf   = bc;
      }
    }

    // start-up case, all frame buffers are zero
    //
    if (num_zero == Archon::nbufs) {
      newestframe = 0;
      newestbuf   = 0;
    }

    /**
     * save index of newest buffer. From this we can find the newest frame, etc.
     */
    this->frame.index = newestbuf;
    this->frame.frame = newestframe;

    // Index of next frame is this->frame.index+1 
    // except for start-up case (when it is 0) and
    // wrapping to 0 when it reaches the maximum number of active buffers.
    //
    this->frame.next_index = this->frame.index + 1;
    if ( this->frame.next_index >= this->camera_info.activebufs ) {
      this->frame.next_index = 0;
    }

    // startup condition for next_frame
    //
    if ( ( this->frame.bufframen[ this->frame.index ] ) == 1 && this->frame.bufcomplete[ this->frame.index ] == 0 ) {
      this->frame.next_index = 0;
    }

//logwrite( function, "[TIMESTAMP] stop" );
    return(error);
  }
  /**************** Archon::Interface::get_frame_status ***********************/


  /**************** Archon::Interface::print_frame_status *********************/
  /**
   * @fn     print_frame_status
   * @brief  print the Archon frame buffer status
   * @param  none
   * @return none
   *
   * Writes the Archon frame buffer status to the log file,
   * I.E. information in this->frame structure, obtained from
   * the "FRAME" command. See Archon::Interface::get_frame_status()
   *
   */
  void Interface::print_frame_status() {
    const std::string function("Archon::Interface::print_frame_status");
    std::stringstream message;
    int bufn;
    char statestr[Archon::nbufs][4];

    // write as log message
    //
    message.str(""); message << "    buf base       rawoff     frame ready lines rawlines rblks width height state";
    logwrite(function, message.str());
    message.str(""); message << "    --- ---------- ---------- ----- ----- ----- -------- ----- ----- ------ -----";
    logwrite(function, message.str());
    message.str("");
    for (bufn=0; bufn < Archon::nbufs; bufn++) {
      memset(statestr[bufn], '\0', 4);
      if ( (this->frame.rbuf-1) == bufn)   strcat(statestr[bufn], "R");
      if ( (this->frame.wbuf-1) == bufn)   strcat(statestr[bufn], "W");
      if ( this->frame.bufcomplete[bufn] ) strcat(statestr[bufn], "C");
    }
    for (bufn=0; bufn < Archon::nbufs; bufn++) {
      message << std::setw(3) << (bufn==this->frame.index ? "-->" : "") << " ";                       // buf
      message << std::setw(3) << bufn+1 << " ";
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frame.bufbase[bufn] << " ";                                                    // base
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frame.bufrawoffset[bufn] << " ";                                               // rawoff
      message << std::setfill(' ') << std::setw(5) << std::dec << this->frame.bufframen[bufn] << " "; // frame
      message << std::setw(5) << this->frame.bufcomplete[bufn] << " ";                                // ready
      message << std::setw(5) << this->frame.buflines[bufn] << " ";                                   // lines
      message << std::setw(8) << this->frame.bufrawlines[bufn] << " ";                                // rawlines
      message << std::setw(5) << this->frame.bufrawblocks[bufn] << " ";                               // rblks
      message << std::setw(5) << this->frame.bufwidth[bufn] << " ";                                   // width
      message << std::setw(6) << this->frame.bufheight[bufn] << " ";                                  // height
      message << std::setw(5) << statestr[bufn];                                                      // state
      logwrite(function, message.str());
      message.str("");
    }
    return;
  }
  /**************** Archon::Interface::print_frame_status *********************/


  /**************** Archon::Interface::lock_buffer ****************************/
  /**
   * @fn     lock_buffer
   * @brief  lock Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::lock_buffer(int buffer) {
    const std::string function("Archon::Interface::lock_buffer");
    std::stringstream message;
    std::stringstream sscmd;

    sscmd.str("");
    sscmd << "LOCK" << buffer;
    if ( this->archon_cmd(sscmd.str()) ) {
      message.str(""); message << "ERROR: sending Archon command to lock frame buffer " << buffer;
      logwrite( function, message.str() );
      return(ERROR);
    }
    return (NO_ERROR);
  }
  /**************** Archon::Interface::lock_buffer ****************************/


  /**************** Archon::Interface::get_timer ******************************/
  /** 
   * @fn     get_timer
   * @brief  read the 64 bit interal timer from the Archon controller
   * @param  *timer pointer to type unsigned long int
   * @return errno on error, 0 if okay.
   *
   * Sends the "TIMER" command to Archon, reads back the reply, and stores the
   * value as (unsigned long int) in the argument variable pointed to by *timer.
   *
   * This is an internal 64 bit timer/counter. One tick of the counter is 10 ns.
   *
   */
  long Interface::get_timer(unsigned long int *timer) {
    const std::string function("Archon::Interface::get_timer");
    std::string reply;
    std::stringstream message, timer_ss;
    std::vector<std::string> tokens;
    long error;

    // send TIMER command to get frame buffer status
    //
    if ( (error = this->archon_cmd(TIMER, reply)) != NO_ERROR ) {
      return(error);
    }

    Tokenize(reply, tokens, "=");                   // Tokenize the reply

    // Reponse should be "TIMER=xxxx\n" so there needs
    // to be two tokens
    //
    if (tokens.size() != 2) {
      message.str(""); message << "unrecognized timer response: " << reply << ". Expected TIMER=xxxx";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // Second token must be a hexidecimal string
    //
    std::string timer_str = tokens[1]; 
    try { timer_str.erase(timer_str.find("\n"), 1); } catch(...) { }  // remove newline
    if (!std::all_of(timer_str.begin(), timer_str.end(), ::isxdigit)) {
      message.str(""); message << "unrecognized timer value: " << timer_str << ". Expected hexadecimal string";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // convert from hex string to integer and save return value
    //
    timer_ss << std::hex << tokens[1];
    timer_ss >> *timer;
    return(NO_ERROR);
  }
  /**************** Archon::Interface::get_timer ******************************/


  /**************** Archon::Interface::fetch **********************************/
  /**
   * @fn     fetch
   * @brief  fetch Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::fetch(uint64_t bufaddr, uint32_t bufblocks) {
    debug( "FETCH_ENTRY frame="+std::to_string(this->lastframe) );
    const std::string function("Archon::Interface::fetch");
    std::stringstream message;
    uint32_t maxblocks = (uint32_t)(1.5E9 / this->camera_info.activebufs / 1024 );
    uint64_t maxoffset = this->frame.bufbase[this->frame.index];
    uint64_t maxaddr = maxoffset + maxblocks;

    if ( bufaddr > maxaddr ) {
      message.str(""); message << "fetch Archon buffer requested address 0x" << std::hex << bufaddr << " exceeds 0x" << maxaddr;
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }
    if ( bufblocks > maxblocks ) {
      message.str(""); message << "fetch Archon buffer requested blocks 0x" << std::hex << bufblocks << " exceeds 0x" << maxblocks;
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    std::stringstream sscmd;
    sscmd << "FETCH"
          << std::setfill('0') << std::setw(8) << std::hex
          << bufaddr
          << std::setfill('0') << std::setw(8) << std::hex
          << bufblocks;
    std::string scmd = sscmd.str();

    std::transform( scmd.begin(), scmd.end(), scmd.begin(), ::toupper );  // make uppercase

    // Sending archon_cmd( FETCH ) will set the archon busy flag and not clear it.
    // If there's an error then archon_cmd() probably cleared it but it's OK to
    // make sure that it's cleared here.
    //
    if ( this->archon_cmd( scmd ) == ERROR ) {
      logwrite( function, "ERROR: sending FETCH command. Aborting read." );
      this->archon_busy.clear();                                            // clear busy flag
      this->archon_cmd(UNLOCK);                                             // unlock all buffers
      return ERROR;
    }

    message.str(""); message << "reading " << (this->camera_info.frame_type==Camera::FRAME_RAW?"raw":"image") << " with " << scmd;
    logwrite(function, message.str());
    debug( "FETCH_EXIT frame="+std::to_string(this->lastframe) );
    return NO_ERROR;
  }
  /**************** Archon::Interface::fetch **********************************/


  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @fn     read_frame
   * @brief  read latest Archon frame buffer
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This is function is overloaded.
   *
   * This version, with no parameter, is the one that is called by the server.
   * The decision is made here if the frame to be read is a RAW or an IMAGE 
   * frame based on this->camera_info.current_observing_mode, then the 
   * overloaded version of read_frame(frame_type) is called with the appropriate 
   * frame type of IMAGE or RAW.
   *
   * This function WILL call write_frame(...) to write data after reading it.
   *
   */
  long Interface::read_frame() {
    const std::string function("Archon::Interface::read_frame");
    std::stringstream message;
    long error = NO_ERROR;

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

    int rawenable = this->modemap[this->camera_info.current_observing_mode].rawenable;

    if (rawenable == -1) {
      this->camera.log_error( function, "RAWENABLE is undefined" );
      return ERROR;
    }

    // RAW-only
    //
    if (this->camera_info.current_observing_mode == "RAW") {              // "RAW" is the only reserved mode name

      // the RAWENABLE parameter must be set in the ACF file, in order to read RAW data
      //
      if (rawenable==0) {
        this->camera.log_error( function, "observing mode is RAW but RAWENABLE=0 -- change mode or set RAWENABLE?" );
        return ERROR;
      }
      else {
        error = this->read_frame(Camera::FRAME_RAW);                              // read raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: reading raw frame" ); return error; }
        error = this->write_frame();                                              // write raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: writing raw frame" ); return error; }
      }
    }

    // IMAGE, or IMAGE+RAW
    // mex was already set = true in the expose function
    //
    else {
      error = this->read_frame(Camera::FRAME_IMAGE);                              // read image frame
      if ( error != NO_ERROR ) { logwrite( function, "ERROR: reading image frame" ); return error; }
      error = this->write_frame();                                                // write image frame
      if ( error != NO_ERROR ) { logwrite( function, "ERROR: writing image frame" ); return error; }

      // If mode is not RAW but RAWENABLE=1, then we will first read an image
      // frame (just done above) and then a raw frame (below). To do that we
      // must switch to raw mode then read the raw frame. Afterwards, switch back
      // to the original mode, for any subsequent exposures..
      //
      if (rawenable == 1) {
#ifdef LOGLEVEL_DEBUG
        logwrite(function, "[DEBUG] rawenable is set -- IMAGE+RAW file will be saved");
        logwrite(function, "[DEBUG] switching to mode=RAW");
#endif
        std::string orig_mode = this->camera_info.current_observing_mode; // save the original mode so we can come back to it
        error = this->set_camera_mode("raw");                             // switch to raw mode
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: switching to raw mode" ); return error; }

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] calling read_frame(Camera::FRAME_RAW) if error=0"; logwrite(function, message.str());
#endif
        error = this->read_frame(Camera::FRAME_RAW);                      // read raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: reading raw frame" ); return error; }
#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] calling write_frame() for raw data if error=0"; logwrite(function, message.str());
#endif
        error = this->write_frame();                                      // write raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: writing raw frame" ); return error; }
#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] switching back to original mode if error=0"; logwrite(function, message.str());
#endif
        error = this->set_camera_mode(orig_mode);                         // switch back to the original mode
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: switching back to previous mode" ); return error; }
      }
    }

    return error;
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @brief      read latest Archon frame buffer
   * @param[in]  frame_type
   * @return     ERROR or NO_ERROR
   *
   * This is the overloaded read_frame function which accepts the frame_type argument.
   * This is called only by this->read_frame() to perform the actual read of the
   * selected frame type.
   *
   * No write takes place here!
   *
   */
  long Interface::read_frame(Camera::frame_type_t frame_type) {
    const std::string function("Archon::Interface::read_frame");
    std::stringstream message;

    try {
      char *ptr=this->image_ring.at(this->ringcount);
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] ringcount=" << std::dec << this->ringcount << " address of ptr=" << std::hex << (void*)ptr;
      logwrite( function, message.str() );
#endif
      return this->read_frame(frame_type, ptr, this->ringcount);  //TODO not sure this is correct for all cases
    }
    catch ( std::out_of_range & ) {
      message << "ringcount " << this->ringcount << " out of range addressing image_ring.size=" << this->image_ring.size();
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @brief      read latest Archon frame buffer
   * @param[in]  frame_type camera frame type is FRAME_RAW or FRAME_IMAGE
   * @param[out] ptr_in     char pointer of where to put data, returned for multiple calls
   * @return     ERROR or NO_ERROR
   *
   * This is the overloaded read_frame function for data cubes because it accepts
   * a pointer. This is required because each slice of a data cube is a separate read,
   * so we want to keep track of where the last frame ended. The pointer is returned
   * and passed back in for repeated calls.
   *
   * No write takes place here!
   *
   */
  long Interface::read_frame( Camera::frame_type_t frame_type, char* &ptr_in, int ringcount_in ) {
    debug( "READ_FRAME_ENTRY frame="+std::to_string(this->lastframe) );
    const std::string function("Archon::Interface::read_frame");
    std::stringstream message;
    int retval;
    int bufready;
    char check[5], header[5];
    int bytesread, totalbytesread, toread;
    uint64_t bufaddr;
    unsigned int block, bufblocks=0;
    long error = ERROR;
    int num_detect = this->modemap[this->camera_info.current_observing_mode].geometry.num_detect;

    this->camera_info.frame_type = frame_type;

    // Check that image buffer is prepared
    //
    try {
      if ( (this->image_ring.at(ringcount_in) == nullptr)        ||
           (this->ringdata_allocated.at(ringcount_in) == 0) ||
           (this->ringdata_allocated.at(ringcount_in) != this->image_data_bytes * this->camera_info.cubedepth) ) {
        message.str(""); message << "image buffer not ready."
                                 << " ringdata_allocated[" << ringcount_in << "]=" << this->ringdata_allocated.at(ringcount_in)
                                 << " image_data_bytes=" << this->image_data_bytes
                                 << " cubedepth=" << this->camera_info.cubedepth;
        this->camera.log_error(function, message.str());
        return(ERROR);
      }

#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] frame_type=" << frame_type
                               << " image_ring[" << ringcount_in << "]=" << std::hex << (void*)this->image_ring.at(ringcount_in)
                               << " ringdata_allocated[" << ringcount_in << "]=" << std::dec << this->ringdata_allocated.at(ringcount_in)
                               << " image_data_bytes=" << this->image_data_bytes
                               << " cubedepth=" << this->camera_info.cubedepth;
      logwrite( function, message.str() );
#endif
    }
    catch ( std::out_of_range & ) {
      message.str(""); message << "ringcount_in " << ringcount_in
                               << " out of range addressing image_ring.size=" << this->image_ring.size()
                               << " or ringdata_allocated.size=" << this->ringdata_allocated.size();
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // Archon buffer number of the last frame read into memory
    //
    bufready = this->frame.index + 1;

    if (bufready < 1 || bufready > this->camera_info.activebufs) {
      message.str(""); message << "invalid Archon buffer " << bufready << " requested. Expected {1:" << this->camera_info.activebufs << "}";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    message.str(""); message << "will read " << (frame_type == Camera::FRAME_RAW ? "raw" : "image")
                             << " data from Archon controller buffer " << bufready << " frame " << this->frame.frame
                             << " into buffer " << (void*)ptr_in;
    logwrite(function, message.str());

    // Lock the frame buffer before reading it
    //
    if ( this->lock_buffer(bufready) == ERROR) { logwrite( function, "ERROR locking frame buffer" ); return (ERROR); }

    // Send the FETCH command to read the memory buffer from the Archon backplane.
    // Archon replies with one binary response per requested block. Each response
    // has a message header.
    //
    switch (frame_type) {
      case Camera::FRAME_RAW:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index] + this->frame.bufrawoffset[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks = (unsigned int) floor( (this->camera_info.image_memory + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      case Camera::FRAME_IMAGE:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks =
        (unsigned int) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      default:  // should be impossible
        message.str(""); message << "unknown frame type specified: " << frame_type << ": expected FRAME_RAW | FRAME_IMAGE";
        this->camera.log_error( function, message.str() );
        return(ERROR);
        break;
    }

    message.str(""); message << "will read " << std::dec << this->camera_info.image_memory << " bytes "
                             << "0x" << std::uppercase << std::hex << bufblocks << " blocks from bufaddr=0x" << bufaddr
                             << " into buffer " << (void*)ptr_in;
    logwrite(function, message.str());

    // send the FETCH command.
    // This will set the archon_busy flag, but not clear it (except on error).
    //
    error = this->fetch(bufaddr, bufblocks);

    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR: fetching Archon buffer" );
      return error;
    }

    // Read the data from the connected socket into memory, one block at a time
    //
    totalbytesread = 0;
//  std::cerr << "reading bytes: ";
    for (block=0; block<bufblocks; block++) {

      // Are there data to read?
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) { message.str(""); message << "Poll timeout waiting for Archon frame data"; error = ERROR;   }  // TODO should error=TIMEOUT?
        if (retval<0)  { message.str(""); message << "Poll error waiting for Archon frame data";   error = ERROR;   }
        if ( error != NO_ERROR ) this->camera.log_error( function, message.str() );
        break;                         // breaks out of for loop
      }

      // Wait for a block+header Bytes to be available
      // (but don't wait more than 1 second -- this should be tens of microseconds or less)
      //
      auto start = std::chrono::steady_clock::now();             // start a timer now

      while ( this->archon.Bytes_ready() < (BLOCK_LEN+4) ) {
        auto now = std::chrono::steady_clock::now();             // check the time again
        std::chrono::duration<double> diff = now-start;          // calculate the duration
        if (diff.count() > 1) {                                  // break while loop if duration > 1 second
//        std::cerr << "\n";
          this->camera.log_error( function, "timeout waiting for data from Archon" );
          error = ERROR;
          break;                       // breaks out of while loop
        }
      }
      if ( error != NO_ERROR ) break;  // needed to also break out of for loop on error

      // Check message header
      //
      SNPRINTF(check, "<%02X:", this->msgref);
      if ( (retval=this->archon.Read(header, 4)) != 4 ) {
        message.str(""); message << "code " << retval << " reading Archon frame header";
        this->camera.log_error( function, message.str() );
        error = ERROR;
        break;                         // break out of for loop
      }

//    message.str(""); message << "[TRACE_FETCH] msgref=" << check << " header=" << header;
//    logwrite( function, message.str() );

      if (header[0] == '?') {  // Archon retured an error
        message.str(""); message << "Archon returned \'?\' reading " << (frame_type==Camera::FRAME_RAW?"raw ":"image ") << " data";
        this->camera.log_error( function, message.str() );
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;                         // break out of for loop
      }
      else if (strncmp(header, check, 4) != 0) {
        message.str(""); message << "Archon command-reply mismatch reading " << (frame_type==Camera::FRAME_RAW?"raw ":"image ")
                                 << " data. header=" << header << " check=" << check;
        this->camera.log_error( function, message.str() );
        error = ERROR;
        break;                         // break out of for loop
      }

      // Read the frame contents
      //
      bytesread = 0;
      do {
        toread = BLOCK_LEN - bytesread;
        if ( (retval=this->archon.Read(ptr_in, (size_t)toread)) > 0 ) {
          bytesread += retval;         // this will get zeroed after each block
          totalbytesread += retval;    // this won't (used only for info purposes)
//        std::cerr << std::setw(10) << totalbytesread << "\b\b\b\b\b\b\b\b\b\b";
          ptr_in += retval;            // advance pointer
        }
      } while (bytesread < BLOCK_LEN);

    } // end of loop: for (block=0; block<bufblocks; block++)

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] ringcount_in=" << std::dec << ringcount_in << " after reading, ptr=" << std::hex << (void*)ptr_in;
    logwrite( function, message.str() );
#endif

    // Archon has sent its data so clear the archon busy flag to
    // allow other threads to access the Archon now.
    //
    this->archon_busy.clear();

//  std::cerr << std::setw(10) << totalbytesread << " complete\n";   // display progress on same line of std err

    // If we broke out of the for loop for an error then report incomplete read
    //
    if ( error==ERROR || block < bufblocks) {
      message.str(""); message << "incomplete frame read " << std::dec 
                               << totalbytesread << " bytes: " << block << " of " << bufblocks << " 1024-byte blocks";
      logwrite( function, message.str() );
      this->print_frame_status();
    }

    // Unlock the frame buffer
    //
    if (error == NO_ERROR) error = this->archon_cmd(UNLOCK);

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      message.str(""); message << "successfully read " << std::dec << totalbytesread << (frame_type==Camera::FRAME_RAW?" raw":" image")
                               << " bytes (0x" << std::uppercase << std::hex << bufblocks << " blocks) from Archon controller";
      logwrite(function, message.str());
    }
    // Throw an error for any other errors
    //
    else {
      logwrite( function, "ERROR: reading Archon camera data to memory!" );
    }
    debug( "READ_FRAME_EXIT frame="+std::to_string(this->lastframe) );
    return error;
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::write_frame ****************************/
  /**
   * @fn     write_frame
   * @brief  creates a FITS_file object to write the image_data buffer to disk
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * A FITS_file object is created here to write the data. This object MUST remain
   * valid while any (all) threads are writing data, so the write_data function
   * will keep track of threads so that it doesn't terminate until all of its 
   * threads terminate.
   *
   * The camera_info class was copied into fits_info when the exposure was started,  //TODO I've un-done this.
   * so use fits_info from here on out.                                              //TODO Don't use fits_info right now.
   *                                                                                 //TODO Only using camera_info
   */
  long Interface::write_frame( ) {
    this->camera.log_error( "Archon::Interface::write_frame", "you shouldn't be using this!" );
    return ERROR;
  }
  long Interface::write_frame( int ringcount_in ) {
    debug( "WRITE_FRAME_ENTRY frame="+std::to_string(this->lastframe)+ " ring="+std::to_string(ringcount_in) );
    const std::string function("Archon::Interface::write_frame");
    std::stringstream message;
    uint32_t   *cbuf32;                  //!< used to cast char buf into 32 bit int
    uint16_t   *cbuf16;                  //!< used to cast char buf into 16 bit int
//  uint16_t   *cdsbuf16;                //!< used to cast char buf into 16 bit int
    int16_t    *cbuf16s;                 //!< used to cast char buf into 16 bit int
//  int16_t    *cdsbuf16s;               //!< used to cast char buf into 16 bit int
    long        error=NO_ERROR;

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

//  message.str(""); message << "writing " << this->fits_info.bitpix << "-bit data from memory to disk";  //TODO
    message.str(""); message << "writing " << this->camera_info.bitpix << "-bit data from memory to disk";
    logwrite(function, message.str());

    // The Archon sends four 8-bit numbers per pixel. To convert this into something usable,
    // cast the image buffer into integers. Handled differently depending on bits per pixel.
    //
    // *** ONLY bitpix=16 IS USED FOR NIRC2 ***
    //
    switch (this->camera_info.bitpix) {

      // convert four 8-bit values into a 32-bit value and scale by 2^16
      //
      case 32: {
        cbuf32 = (uint32_t *)this->image_data;                  // cast here to 32b

        // Write each amplifier as a separate extension
        //
        if ( this->camera.mexamps() ) {
#ifdef LOGLEVEL_DEBUG
          logwrite( function, "[DEBUG] will write each amplifier as a separate extension" );
#endif
          float *fext = nullptr;

          for ( int ext=0; ext < (int)this->camera_info.amp_section.size(); ext++ ) {
            try {
              // get the coordinates of this amplifier extension
              //
              long x1 = this->camera_info.amp_section.at(ext).at(0);
              long x2 = this->camera_info.amp_section.at(ext).at(1);
              long y1 = this->camera_info.amp_section.at(ext).at(2);
              long y2 = this->camera_info.amp_section.at(ext).at(3);

              // assign this amplifier section as the region of interest
              //
              this->camera_info.region_of_interest[0] = x1;
              this->camera_info.region_of_interest[1] = x2;
              this->camera_info.region_of_interest[2] = y1;
              this->camera_info.region_of_interest[3] = y2;

              // This call to set_axes() is to set the axis_pixels, axes, and section_size,
              // needed for the FITS writer
              //
              error = this->camera_info.set_axes();

#ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] x1=" << x1 << " x2=" << x2 << " y1=" << y1 << " y2=" << y2;
              logwrite( function, message.str() );
              message.str(""); message << "[DEBUG] axes[0]=" << this->camera_info.axes[0] << " axes[1]=" << this->camera_info.axes[1];
              logwrite( function, message.str() );
#endif

              // allocate the number of pixels needed for this amplifier extension
              //
              long ext_size = (x2-x1+1) * (y2-y1+1);
              fext = new float[ ext_size ]{};

#ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] allocated " << ext_size << " pixels for extension "
                                       << this->camera_info.extension.load( std::memory_order_seq_cst )+1;
              logwrite( function, message.str() );
#endif

              // copy this amplifier from the main cbuf32,
              // at the same time right-shift the requested number of bits
              //
              long pix=0;
              long ncols=this->camera_info.detector_pixels[0];  // PIXELCOUNT
              for ( long row=y1-1; row<y2; row++ ) {
                for ( long col=x1-1; col<x2; col++ ) {
                  fext[pix++] = (float)( cbuf32[ row*ncols + col ] >> this->n_hdrshift );
                }
              }

#ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] calling xfits_file.write_image( ) for extension "
                                       << this->camera_info.extension.load( std::memory_order_seq_cst )+1;
              logwrite( function, message.str() );
#endif

              logwrite( function, "ERROR THIS SHOULD NOT BE HAPPENING" );
              return ERROR;
//            error = this->xfits_file.write_image(fext, this->camera_info); // write the image to disk
              this->camera_info.extension.fetch_add(1);                     // atomically increment extension for multi-extension files
              if ( fext != nullptr ) { delete [] fext; fext=nullptr; }      // dynamic object not automatic so must be destroyed
            }
            catch( std::out_of_range & ) {
              message.str(""); message << "ERROR: " << ext << " is a bad extension number";
              logwrite( function, message.str() );
              if ( fext != nullptr ) { delete [] fext; fext=nullptr; }      // dynamic object not automatic so must be destroyed
              error = ERROR;
            }
          }
        }  // end if this->camera.mexamps()

        // Write all amplifiers to the same extension
        //
        else {
          float *fbuf = nullptr;
//        fbuf = new float[ this->fits_info.section_size ];       // allocate a float buffer of same number of pixels for scaling  //TODO
          fbuf = new float[ this->camera_info.section_size ]{};   // allocate a float buffer of same number of pixels for scaling

//        for (long pix=0; pix < this->fits_info.section_size; pix++)   //TODO
          for (long pix=0; pix < this->camera_info.section_size; pix++) {
            fbuf[pix] = (float) ( cbuf32[pix] >> this->n_hdrshift ); // right shift the requested number of bits
          }

//        error = xfits_file.write_image(fbuf, this->fits_info);   // write the image to disk //TODO
//        error = this->xfits_file.write_image(fbuf, this->camera_info); // write the image to disk
          if ( error != NO_ERROR ) { this->camera.log_error( function, "writing 32-bit image to disk" ); }
          if (fbuf != nullptr) {
            delete [] fbuf;
            fbuf = nullptr;
          }
        }
        break;
      }

      // convert four 8-bit values into 16 bit values
      //
      case 16: {
        // *** ONLY USHORT IS USED FOR NIRC2 ***
        if (this->camera_info.datatype == USHORT_IMG) {                    // raw
          cbuf16   = (uint16_t *)this->work_ring.at(ringcount_in);         // cast to 16b unsigned int
//        error = this->xfits_file.write_image(cbuf16, this->camera_info);  // write the image to disk
          error = this->__fits_file->write_image( cbuf16,
                                                  get_timestamp(),
                                                  this->camera_info.extension.load(),
                                                  this->camera_info
                                                );
          if ( this->camera_info.iscds ) {
//          cdsbuf16 = (uint16_t *)this->cds_ring.at(ringcount_in);          // cast to 16b unsigned int
//          error |= this->xcds_file.write_image( cdsbuf16, this->cds_info ); // write the cds image to disk
          }
          if ( error != NO_ERROR ) { this->camera.log_error( function, "writing 16-bit unsigned image to disk" ); }
        }
        else
        if (this->camera_info.datatype == SHORT_IMG) {
          cbuf16s   = (int16_t *)this->work_ring.at(ringcount_in);             // cast to 16b signed int
//        error = this->xfits_file.write_image( cbuf16s, this->camera_info );   // write the image to disk
          if ( this->camera_info.iscds ) {
//          cdsbuf16s = (int16_t *)this->cds_ring.at(ringcount_in);            // cast to 16b signed int
//          error |= this->xcds_file.write_image( cdsbuf16s, this->cds_info );  // write the cds image to disk
	  }
          if ( error != NO_ERROR ) { this->camera.log_error( function, "writing 16-bit signed image to disk" ); }
        }
        else {
          message.str(""); message << "unsupported 16 bit datatype " << this->camera_info.datatype;
          this->camera.log_error( function, message.str() );
          error = ERROR;
        }
        break;
      }

      // shouldn't happen
      //
      default:
        message.str(""); message << "unrecognized bits per pixel: " << this->camera_info.bitpix;
        this->camera.log_error( function, message.str() );
        error = ERROR;
        break;
    }

#ifdef NOT_READY_YET
    if ( this->cds_info.iscds ) {
      int16_t*  cdsbuf16s;                //!< used to cast 
      uint16_t* cdsbuf16u;                //!< used to cast 
      int32_t*  cdsbuf32s;                //!< used to cast 
      uint32_t* cdsbuf32u;                //!< used to cast 
      switch ( this->cds_info.bitpix ) {
        case 32:
          if ( this->camera_info.datatype == LONG_IMG ) {
            cdsbuf32s = (int32_t *)this->cds_ring.at(ringcount_in);               // cast to 32b signed int
//          error    |= this->xcds_file.write_image( cdsbuf32s, this->cds_info );  // write the cds image to disk
	  }
	  else {
	    message.str(""); message << "unrecognized bitpix " << this->cds_info.bitpix << " for datatype LONG_IMG";
	    this->camera.log_error( function, message.str());
	    error = ERROR;
	  }
          break;
        case 16:
          if ( this->camera_info.datatype == USHORT_IMG ) {                    // raw
            cdsbuf16 = (uint16_t *)this->cds_ring.at(ringcount_in);          // cast to 16b unsigned int
//          error |= this->xcds_file.write_image( cdsbuf16, this->cds_info ); // write the cds image to disk
	  }
          break;
	default:
          message.str(""); message << "cds unrecognized bits per pixel: " << this->cds_info.bitpix;
          this->camera.log_error( function, message.str() );
          error = ERROR;
          break;
      }
    }
#endif

    // Things to do after successful write
    //
    if ( error == NO_ERROR ) {
      if ( this->camera.mex() ) {
        this->camera_info.extension.fetch_add(1);                     // atomically increment extension for multi-extension files
        this->cds_info.extension.fetch_add(1);                        // atomically increment extension for multi-extension files
//      message.str(""); message << "DATACUBE:" << this->camera_info.extension << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
//      this->camera.async.enqueue( message.str() );
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }
      logwrite(function, "frame write complete");
    }
    else {
      logwrite( function, "ERROR: writing image" );
    }

    debug( "WRITE_FRAME_EXIT frame="+std::to_string(this->lastframe)+ " ring="+std::to_string(ringcount_in) );
    return(error);
  }
  /**************** Archon::Interface::write_frame ****************************/


  /**************** Archon::Interface::write_config_key ***********************/
  /**
   * @fn     write_config_key
   * @brief  write a configuration KEY=VALUE pair to the Archon controller
   * @param  key
   * @param  newvalue
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::write_config_key( const char *key, const char *newvalue, bool &changed ) {
    const std::string function("Archon::Interface::write_config_key");
    std::stringstream message, sscmd;
    long error=NO_ERROR;

    if ( key==NULL || newvalue==NULL ) {
      error = ERROR;
      this->camera.log_error( function, "key|value cannot have NULL" );
    }

    else

    if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      message.str(""); message << "requested key " << key << " not found in configmap";
      this->camera.log_error( function, message.str() );
    }

    else

    /**
     * If no change in value then don't send the command
     */
    if ( this->configmap[key].value == newvalue ) {
      error = NO_ERROR;
      message.str(""); message << "config key " << key << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());
    }

    else

    /**
     * Format and send the Archon WCONFIG command
     * to write the KEY=VALUE pair to controller memory
     */
    {
      sscmd << "WCONFIG"
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->configmap[key].line
            << key
            << "="
            << newvalue;
      message.str(""); message << "sending: archon_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;               // save newvalue in the STL map
        changed = true;
      }
      else {
        message.str(""); message << "ERROR: config key=value: " << key << "=" << newvalue << " not written";
        logwrite( function, message.str() );
      }
    }
    return(error);
  }

  long Interface::write_config_key( const char *key, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_config_key(key, newvaluestr.str().c_str(), changed) );
  }
  /**************** Archon::Interface::write_config_key ***********************/


  /**************** Archon::Interface::write_parameter ************************/
  /**
   * @fn     write_parameter
   * @brief  write a parameter to the Archon configuration memory
   * @param  paramname
   * @param  newvalue
   * @return NO_ERROR or ERROR
   *
   * After writing a parameter, requires an APPLYALL or LOADPARAMS command
   *
   */
  long Interface::write_parameter( const char *paramname, const char *newvalue, bool &changed ) {
    const std::string function("Archon::Interface::write_parameter");
    std::stringstream message, sscmd;
    long error=NO_ERROR;

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] paramname=" << paramname << " value=" << newvalue;
    logwrite( function, message.str() );
#endif

    if ( paramname==NULL || newvalue==NULL ) {
      error = ERROR;
      this->camera.log_error( function, "paramname|value cannot have NULL" );
    }

    else

    if ( this->parammap.find(paramname) == this->parammap.end() ) {
      error = ERROR;
      message.str(""); message << "parameter \"" << paramname << "\" not found in parammap";
      this->camera.log_error( function, message.str() );
    }

    /**
     * If no change in value then don't send the command
     */
    if ( error==NO_ERROR && this->parammap[paramname].value == newvalue ) {
      error = NO_ERROR;
      message.str(""); message << "parameter " << paramname << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());
    }

    else

    /**
     * Format and send the Archon command WCONFIGxxxxttt...ttt
     * which writes the text ttt...ttt to configuration line xxx (hex)
     * to controller memory.
     */
    if (error==NO_ERROR) {
      sscmd << "WCONFIG" 
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->parammap[paramname].line
            << this->parammap[paramname].key
            << "="
            << this->parammap[paramname].name
            << "="
            << newvalue;
      message.str(""); message << "sending archon_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if ( error == NO_ERROR ) {
        this->parammap[paramname].value = newvalue;            // save newvalue in the STL map
        changed = true;
      }
      else logwrite( function, "ERROR: sending WCONFIG command" );
    } 
    
    return(error);
  } 
  
  long Interface::write_parameter( const char *paramname, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_parameter(paramname, newvaluestr.str().c_str(), changed) );
  }

  long Interface::write_parameter( const char *paramname, const char *newvalue ) {
    bool dontcare = false;
    return( write_parameter(paramname, newvalue, dontcare) );
  }

  long Interface::write_parameter( const char *paramname, int newvalue ) {
    bool dontcare = false;
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_parameter(paramname, newvaluestr.str().c_str(), dontcare) );
  }
  /**************** Archon::Interface::write_parameter ************************/


  /***** Archon::Interface::get_parammap_value ********************************/
  /**
   * @brief      get the VALUE of a given parameter
   * @param[in]  param_in   parameter name to read
   * @param[out] value_out  reference to value out
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::get_parammap_value( std::string param_in, long& value_out ) {
    const std::string function("Archon::Interface::get_parammap_value");
    std::stringstream message;
    std::string retstring;
    long error = NO_ERROR;

    if ( this->parammap.find( param_in ) == this->parammap.end() ) {
      error = ERROR;
      message.str(""); message << "parameter \"" <<  param_in << "\" not found in parammap";
      this->camera.log_error( function, message.str() );
    }
    else {
      try {
        value_out = std::stol( this->parammap[param_in].value );
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "ERROR invalid argument converting value for " << param_in << " to long integer";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "ERROR out of range converting value for " << param_in << " to long integer";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
    }

    return error;
  }
  /***** Archon::Interface::get_parammap_value ********************************/


  /**************** Archon::Interface::get_configmap_value ********************/
  /**
   * @fn     get_configmap_value
   * @brief  get the VALUE from configmap for a givenn KEY and assign to a variable
   * @param  string key_in is the KEY
   * @param  T& value_out reference to variable to contain the VALUE
   * @return ERROR or NO_ERROR
   *
   * This is a template class function so the &value_out reference can be any type.
   * If the key_in KEY is not found then an error message is logged and ERROR is
   * returned, otherwise the VALUE associated with key_in is assigned to &value_out, 
   * and NO_ERROR is returned.
   *
   */
  template <class T>
  long Interface::get_configmap_value(std::string key_in, T& value_out) {
    const std::string function("Archon::Interface::get_configmap_value");
    std::stringstream message;

    if ( this->configmap.find(key_in) != this->configmap.end() ) {
      std::istringstream( this->configmap[key_in].value  ) >> value_out;
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] key=" << key_in << " value=" << value_out << " line=" << this->configmap[key_in].line;
      logwrite(function, message.str());
#endif
      return NO_ERROR;
    }
    else {
      message.str("");
      message << "requested key: " << key_in << " not found in configmap";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }
  }
  /**************** Archon::Interface::get_configmap_value ********************/


  /***** Archon::Interface::add_filename_key **********************************/
  /**
   * @fn         add_filename_key
   * @brief      adds the current filename to the systemkeys database
   *
   * This function is overloaded.
   * When no camera info class object is given then use this->camera_info
   *
   */
  void Interface::add_filename_key() {
    this->add_filename_key( this->camera_info );
  }
  /***** Archon::Interface::add_filename_key **********************************/


  /***** Archon::Interface::add_filename_key **********************************/
  /**
   * @fn         add_filename_key
   * @brief      adds the current filename to the systemkeys database
   * @param[in]  info  reference to camera info object
   *
   * This function is overloaded.
   * The filename is added directly to the systemkeys database of the given
   * camera info class object so be sure not to overwrite the systemkeys db
   * after doing this.
   *
   */
  void Interface::add_filename_key( Camera::Information &info ) {
    std::stringstream keystr;
    auto loc = info.fits_name.find_last_of( "/" );
    std::string filename;
    filename = info.fits_name.substr( loc+1 );
    keystr << "FILENAME=" << filename << "// this filename";
    info.systemkeys.addkey( keystr.str() );
  }
  /***** Archon::Interface::add_filename_key **********************************/


  /***** Archon::Interface::get_status_key ************************************/
  /**
   * @brief      get value for the indicated key from the Archon "STATUS" string
   * @param[in]  key    key to extract from STATUS
   * @param[out] value  value of key
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::get_status_key( std::string key, std::string &value ) {
    const std::string function("Archon::Interface::get_status_key");
    std::stringstream message;
    std::string reply;

    long error = this->archon_cmd( STATUS, reply );  // first the whole reply in one string

    if ( error != NO_ERROR ) return error;

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                   // then each line in a separate token "lines"

    for ( auto line : lines ) {
      Tokenize( line, tokens, "=" );                 // break each line into tokens to get KEY=value
      if ( tokens.size() != 2 ) continue;            // need 2 tokens
      try {
        if ( tokens.at(0) == key ) {                 // looking for the KEY
          value = tokens.at(1);                      // found the KEY= status here
          break;                                     // done looking
        }
        else continue;
      }
      catch (std::out_of_range &) {                  // should be impossible
        this->camera.log_error( function, "token out of range" );
        return(ERROR);
      }
    }

    return( NO_ERROR );
  }
  /***** Archon::Interface::get_status_key ************************************/


  /***** Archon::Interface::temp **********************************************/
  /**
   * @brief      get the backplane temperature
   * @param[out] retstring   contains the backplane temperature
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::temp( std::string &retstring ) {
    const std::string function("Archon::Interface::temp");
    std::stringstream message;

    if ( !this->archon.isconnected() ) {                        // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return( ERROR );
    }

    long error = get_status_key( "BACKPLANE_TEMP", retstring );

    return( error );
  }
  /***** Archon::Interface::temp **********************************************/


  /***** Archon::Interface::fan ***********************************************/
  /**
   * @brief      get the fan tach
   * @param[out] retstring   contains the fan tach
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::fan( std::string &retstring ) {
    const std::string function("Archon::Interface::fan");
    std::stringstream message;

    if ( !this->archon.isconnected() ) {                        // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return( ERROR );
    }

    long error = get_status_key( "FANTACH", retstring );

    return( error );
  }
  /***** Archon::Interface::fan ***********************************************/


  /***** Archon::Interface::overheat ******************************************/
  /**
   * @brief      get the overheat state
   * @param[out] retstring   contains the overheat state
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::overheat( std::string &retstring ) {
    const std::string function("Archon::Interface::overheat");
    std::stringstream message;

    if ( !this->archon.isconnected() ) {                        // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return( ERROR );
    }

    long error = get_status_key( "OVERHEAT", retstring );

    retstring = ( retstring == "1" ? "yes" : "no" );            // change return to "yes" or "no" from "1" or "0"

    return( error );
  }
  /***** Archon::Interface::overheat ******************************************/


  /***** Archon::Interface::tempinfo ******************************************/
  /**
   * @brief      get the temperature info, TEMP, FAN, OVERHEAT
   * @param[out] retstring   return string contains the temperature info
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::tempinfo( std::string &retstring ) {
    const std::string function("Archon::Interface::tempinfo");
    std::stringstream message;
    long error = ERROR;

    if ( !this->archon.isconnected() ) {                        // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return( ERROR );
    }

    std::string value;

    // Get three key=values: BACKPLANE_TEMP, FANTACH, OVERHEAT, and
    // assemble them all into one string.
    //
    error = get_status_key( "BACKPLANE_TEMP", value );
    message << "TEMP=" << ( error==NO_ERROR ? value : "error" );

    error = get_status_key( "FANTACH", value );
    message << " FAN=" << ( error==NO_ERROR ? value : "error" );

    value.clear();
    error = get_status_key( "OVERHEAT", value );
    value = ( value == "1" ? "yes" : "no" );
    message << " OVERHEAT=" << ( error==NO_ERROR ? value : "error" );

    retstring = message.str();

    return( error );
  }
  /***** Archon::Interface::tempinfo ******************************************/


  /***** Archon::Interface::do_power ******************************************/
  /**
   * @brief      set/get the power state
   * @param[in]  state_in    input string contains requested power state
   * @param[out] retstring   return string contains the current power state
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::do_power(std::string state_in, std::string &retstring) {
    const std::string function("Archon::Interface::do_power");
    std::stringstream message;
    long error = ERROR;

    if ( !this->archon.isconnected() ) {                        // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return( ERROR );
    }

    // set the Archon power state as requested
    //
    if ( !state_in.empty() ) {                                  // received something
      std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase
      if ( state_in == "ON" ) {
        error = this->archon_cmd( POWERON );                    // send POWERON command to Archon
        if ( error == NO_ERROR ) std::this_thread::sleep_for( std::chrono::seconds(2) );         // wait 2s to ensure power is stable
      }
      else
      if ( state_in == "OFF" ) {
        error = this->archon_cmd( POWEROFF );                   // send POWEROFF command to Archon
        if ( error == NO_ERROR ) std::this_thread::sleep_for( std::chrono::milliseconds(200) );  // wait 200ms to ensure power is off
      }
      else {
        message.str(""); message << "unrecognized argument " << state_in << ": expected {on|off}";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      if ( error != NO_ERROR ) {
        message.str(""); message << "setting Archon power " << state_in;
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }

    // Read the Archon power state directly from Archon
    //
    std::string power;
    error = get_status_key( "POWER", power );

    if ( error != NO_ERROR ) return( ERROR );

    int status=-1;

    try { status = std::stoi( power ); }
    catch (std::invalid_argument &) {
      this->camera.log_error( function, "unable to convert power status message to integer" );
      return(ERROR);
    }
    catch (std::out_of_range &) {
      this->camera.log_error( function, "power status out of range" );
      return(ERROR);
    }

    // set the power status (or not) depending on the value extracted from the STATUS message
    //
    switch( status ) {
      case -1:                                                  // no POWER token found in status message
        this->camera.log_error( function, "unable to find power in Archon status message" );
        return( ERROR );
      case  0:                                                  // usually an internal error
        this->camera.power_status = "UNKNOWN";
        break;
      case  1:                                                  // no configuration applied
        this->camera.power_status = "NOT_CONFIGURED";
        break;
      case  2:                                                  // power is off
        this->camera.power_status = "OFF";
        break;
      case  3:                                                  // some modules powered, some not
        this->camera.power_status = "INTERMEDIATE";
        break;
      case  4:                                                  // power is on
        this->camera.power_status = "ON";
        break;
      case  5:                                                  // system is in standby
        this->camera.power_status = "STANDBY";
        break;
      default:                                                  // should be impossible
        message.str(""); message << "unknown power status: " << status;
        this->camera.log_error( function, message.str() );
        return( ERROR );
    }

    message.str(""); message << "POWER:" << this->camera.power_status;
    this->camera.async.enqueue( message.str() );

    retstring = this->camera.power_status;

    return(NO_ERROR);
  }
  /***** Archon::Interface::do_power ******************************************/


  /***** Archon::Interface::do_expose *****************************************/
  /**
   * @brief  initiate an exposure
   * @param  nseq_in  if set becomes the number of Archon sequences, passed as EXPOSE parameter
   * @return ERROR or NO_ERROR
   *
   * This function does the following before returning successful completion:
   *  1) trigger an Archon exposure by setting the EXPOSE parameter = nseq_in
   *  2) wait for exposure delay
   *  3) wait for readout into Archon frame buffer
   *  4) read frame buffer from Archon to host
   *  5) write frame to disk
   *
   * Note that this assumes that the Archon ACF has been programmed to automatically
   * read out the detector into the frame buffer after an exposure.
   *
   */
  long Interface::do_expose(std::string nseq_in) {
    debug( "DO_EXPOSE_ENTRY" );
    const std::string function("Archon::Interface::do_expose");
    std::stringstream message;
    long error = NO_ERROR;
    std::string nseqstr;
    int nseq;

    this->camera_info.cmd_start_time = get_timestamp();           // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
    this->camera.clear_abort();                                   // could be cleared by any earlier expose() wrapper

    this->deinterlace_count.store( 0, std::memory_order_seq_cst );
    this->write_frame_count.store( 0, std::memory_order_seq_cst );

    std::string mode = this->camera_info.current_observing_mode;  // local copy for convenience

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

    // When switching from mexamps=true to mexamps=false,
    // simply reset the mode to the current mode in order to
    // reset the image size. 
    //
    // This will need to be revisited once ROI is implemented. // TODO
    //
    if ( !this->camera.mexamps() && ( this->lastmexamps != this->camera.mexamps() ) ) {
      message.str(""); message << "detected change in mexamps -- resetting camera mode to " << mode;
      logwrite( function, message.str() );
      this->set_camera_mode( mode );
    }

    // abortparam is set by the configuration file
    // check to make sure it was set, or else expose won't work
    //
    if (this->abortparam.empty()) {
      message.str(""); message << "ABORT_PARAM not defined in configuration file " << this->config.filename;
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // exposeparam is set by the configuration file
    // check to make sure it was set, or else expose won't work
    //
    if (this->exposeparam.empty()) {
      message.str(""); message << "EXPOSE_PARAM not defined in configuration file " << this->config.filename;
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // If the exposure time or longexposure mode were never set then read them from the Archon.
    // This ensures that, if the client doesn't set these values then the server will have the
    // same default values that the ACF has, rather than hope that the ACF programmer picks
    // their defaults to match mine.
    //
    if ( this->camera_info.exposure_time   == -1 ) {
      logwrite( function, "NOTICE:exptime has not been set--will read from Archon" );
      this->camera.async.enqueue( "NOTICE:exptime has not been set--will read from Archon" );

      // read the Archon configuration memory
      //
      std::string etime;
      if ( read_parameter( "exptime", etime ) != NO_ERROR ) { logwrite( function, "ERROR: reading \"exptime\" parameter from Archon" ); return ERROR; }

      // Tell the server these values
      //
      std::string retval;
      if ( this->exptime( etime, retval ) != NO_ERROR ) { logwrite( function, "ERROR: setting exptime" ); return ERROR; }
    }
    if ( this->camera_info.exposure_factor == -1 ||
         this->camera_info.exposure_unit.empty() ) {
      logwrite( function, "NOTICE:longexposure has not been set--will read from Archon" );
      this->camera.async.enqueue( "NOTICE:longexposure has not been set--will read from Archon" );

      // read the Archon configuration memory
      //
      std::string lexp;
      if ( read_parameter( "longexposure", lexp ) != NO_ERROR ) { logwrite( function, "ERROR: reading \"longexposure\" parameter from Archon" ); return ERROR; }

      // Tell the server these values
      //
      std::string retval;
      if ( this->longexposure( lexp, retval ) != NO_ERROR ) { logwrite( function, "ERROR: setting longexposure" ); return ERROR; }
    }

    // If nseq_in is not supplied then set nseq to 1.
    // Add any pre-exposures onto the number of sequences.
    //
    if ( nseq_in.empty() ) {
      nseq = 1 + this->camera_info.num_pre_exposures;
      nseqstr = std::to_string( nseq );
    }
    else {                                                                 // sequence argument passed in
      try {
        nseq = std::stoi( nseq_in ) + this->camera_info.num_pre_exposures; // test that nseq_in is an integer
        nseqstr = std::to_string( nseq );                                  // before trying to use it
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "unable to convert sequences: " << nseq_in << " to integer";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "sequences " << nseq_in << " outside integer range";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
    }

    switch( this->camera_info.readout_type ) {
      case Archon::READOUT_NIRC2VIDEO:
      case Archon::READOUT_NIRC2:
        this->camera_info.axes[0] = this->camera_info.imwidth;
        this->camera_info.axes[1] = this->camera_info.imheight;
        break;

      case Archon::READOUT_NONE:
        this->camera_info.set_axes();
        break;

      default:
        message.str(""); message << "unknown readout_type " << this->camera_info.readout_type;
        this->camera.log_error( function, message.str() );
        return(ERROR);
        break;
    }
    for (int i=0; i<3; i++) this->camera_info.naxes[i]=this->camera_info.axes[i];  // for Reed's FITS engine

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] axes[0]=" << this->camera_info.axes[0] << " axes[1]=" << this->camera_info.axes[1];
    logwrite( function, message.str() );
#endif

    // Save nseq to the class but
    // use the local variable within this function because it will get decremented.
    //
    this->camera_info.nseq = nseq;

    // Always initialize the extension number because someone could
    // set mex true and then send "expose" without a number.
    //
    this->camera_info.extension.store(0);

    // Save the mex state in camera_info so that the FITS writer can know about it
    //
    this->camera_info.ismex = this->camera.mex();

    error = this->get_frame_status();  // TODO is this needed here?

    if (error != NO_ERROR) {
      logwrite( function, "ERROR: unable to get frame status" );
      return(ERROR);
    }

    // If CDS is requested then prepare the cds_info structure,
    // a copy of the camera_info structure with some modifications.
    // Also spawn a thread that will handle this (TBD).
    //
    if ( this->camera_info.iscds ) {
      this->cds_info = this->camera_info;  // first, copy the camera_info object, then make changes to it
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] cds_info.imheight=" << this->cds_info.imheight << " cds_info.imwidth=" << this->cds_info.imwidth;
      logwrite( function, message.str() );
#endif
      this->cds_info.ismex = false;        // not multi-extension
      this->cds_info.fitscubed = 1;        // and not cubes
      this->cds_info.cubedepth = 1;
      this->cds_info.axes[2] = 1;
      this->cds_info.section_size = this->cds_info.imheight * this->cds_info.imwidth;
      if ( this->camera.coadd() ) {   // need long to handle coadding
        this->cds_info.datatype = LONG_IMG;
        this->cds_info.bitpix = 32;
      }
      std::thread( std::ref( Archon::Interface::dothread_runcds ), this ).detach();
      error = this->alloc_cdsring();
    }
#ifdef LOGLEVEL_DEBUG
    else { logwrite( function, "[DEBUG] iscds=false. alloc_cdsring() was not called!" ); }
#endif

    error |= this->prepare_ring_buffer();
    error |= this->alloc_workring();

    if (error != NO_ERROR) {
      this->camera.log_error( function, "couldn't allocate memory" );
      return(ERROR);
    }

    this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    // Before initiating the exposure, there is a kludge needed for SAMPMODE_SINGLE.
    // This mode is the same as SAMPMODE_RXV with 2 frames, except that NIRC2 only
    // wants 1. So we let the user tell us 1 frame but then we have to tell Archon
    // 2 frames, and then discard the first one so that no one knows.
    //
    if ( this->camera_info.sampmode == SAMPMODE_SINGLE ) nseqstr = std::to_string( 2 );  // override nseq

    //
    // *** initiate the exposure here ***
    //
    error = this->prep_parameter(this->exposeparam, nseqstr);

    if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);
    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR: could not initiate exposure" );
      this->cleanup_memory();
      return( error );
    }
    debug( "EXPOSURE_INITIATED" );
    logwrite(function, "exposure started");

    // get system time and Archon's timer after exposure starts
    // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
    //
    this->camera_info.start_time = get_timestamp();                 // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
    if ( this->get_timer(&this->start_timer) != NO_ERROR ) {        // Archon internal timer (one tick=10 nsec)
      logwrite( function, "ERROR: could not get start time" );
      this->cleanup_memory();
      return( ERROR );
    }
    this->last_frame_timer = this->start_timer;                             // initialize timer, used as reference for wait_for_exposure
    this->camera.set_fitstime(this->camera_info.start_time);                // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
    error=this->camera.get_fitsname( "_unp", this->camera_info.fits_name);  // assemble the FITS filename with added "raw"tag
    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR: couldn't validate fits filename" );
      this->cleanup_memory();
      return( error );
    }

    this->camera_info.systemkeys.keydb = this->systemkeys.keydb;    // copy the systemkeys database object into camera_info

    this->add_filename_key();                                       // add filename to system keys database

    // Prepare the cds info struct if a processed file is requested
    //
    if ( this->camera_info.iscds ) {
      this->cds_info.systemkeys.keydb  = this->systemkeys.keydb;    // copy the systemkeys database object into cds_info
      this->cds_info.start_time = this->camera_info.start_time;     // start time is the same
      error=this->camera.get_fitsname( this->cds_info.fits_name);   // assemble the FITS filename
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: couldn't validate fits filename" );
        this->cleanup_memory();
        return( error );
      }
      this->add_filename_key( this->cds_info );                     // add filename to cds system keys database
    }

    if (this->camera.writekeys_when=="before") this->copy_keydb();  // copy the ACF and userkeys database into camera_info

    // If mode is not "RAW" but RAWENABLE is set then we're going to require a multi-extension format
    // one extension for the image and a separate extension for raw data.
    //
    if ( (mode != "RAW") && (this->modemap[mode].rawenable) ) {
      if ( !this->camera.mex() ) {                                  // if mex not already set then it must be overridden here
        this->camera.async.enqueue( "NOTICE:override mex true" );   // let everyone know
        logwrite( function, "NOTICE:override mex true" );
        this->camera.mex(true);
      }
      this->camera_info.extension.store(0);
    }

    // Open the FITS file now for multi-extensions
    //
    // *** THIS IS THE FITS FILE CREATION USED BY NIRC2 ***
    //
    if ( this->camera.mex() && !this->camera.mexamps() ) {
#ifdef LOGLEVEL_DEBUG
      logwrite( function, "[DEBUG] opening fits file for multi-exposure sequence using multi-extensions" );
#endif
//    error  = this->xfits_file.open_file( (this->camera.writekeys_when=="before"?true:false), this->camera_info );
      this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
      this->__fits_file = std::make_unique<FITS_file<uint16_t>>(true);  // true = multi-extension

      // Also open a CDS file if needed
      //
      if ( this->camera_info.iscds ) {
#ifdef LOGLEVEL_DEBUG
        logwrite( function, "[DEBUG] opening fits file for CDS processed images" );
#endif
//      error |= this->xcds_file.open_file( (this->camera.writekeys_when=="before"?true:false), this->cds_info );
        this->cds_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
        this->__file_cds = std::make_unique<FITS_file<int32_t>>(false);  // false = not multi-extension
      }

      if ( error != NO_ERROR ) {
        this->camera.log_error( function, "couldn't open fits file" );
        this->cleanup_memory();
        return( error );
      }
    }
#ifdef LOGLEVEL_DEBUG
    logwrite( function, "[DEBUG] opened fits file for multi-exposure sequence using multi-extensions" );
#endif

//  //TODO only use camera_info -- don't use fits_info -- is this OK? TO BE CONFIRMED
//  this->fits_info = this->camera_info;                            // copy the camera_info class, to be given to fits writer  //TODO

//  this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    if (nseq > 1) {
      message.str(""); message << "starting sequence of " << nseq << " frames. lastframe=" << this->lastframe;
      logwrite(function, message.str());
    }

    // If not RAW mode then wait for Archon frame buffer to be ready,
    // then read the latest ready frame buffer to the host. If this
    // is a squence, then loop over all expected frames.
    //
    if ( mode != "RAW" ) {                                          // If not raw mode then
      int expcount = 0;                                             // counter used only for tracking pre-exposures

      //
      // -- MAIN SEQUENCE LOOP --
      //
      while (nseq-- > 0) {

        this->camera_info.ncoadd = this->camera_info.nseq - nseq;
        this->cds_info.ncoadd    = this->camera_info.nseq - nseq;

        // prepare and store the appropriate header key for this particular "coadd" (they're not all coadds)
        //
        message.str("");
        switch( this->camera_info.sampmode ) {
          case SAMPMODE_SINGLE:
          case SAMPMODE_CDS:
          case SAMPMODE_MCDS:
            message << "NCOADD=" << this->camera_info.ncoadd << "// coadd number";
            break;
          case SAMPMODE_UTR:
            message << "NRAMP=" << this->camera_info.ncoadd << "// ramp number";
            break;
          case SAMPMODE_RXV:
          case SAMPMODE_RXRV:
            message << "NFRAME=" << this->camera_info.ncoadd << "// frame number";
            break;
        }
        this->extkeys.addkey( message.str() );

        // Wait for any pre-exposures, first the exposure delay then the readout,
        // but then continue to the next because pre-exposures are not read from
        // the Archon's buffer.
        //
        if ( ++expcount <= this->camera_info.num_pre_exposures ) {

          message.str(""); message << "pre-exposure " << expcount << " of " << this->camera_info.num_pre_exposures;
          logwrite( function, message.str() );

          if ( this->camera_info.exposure_time != 0 ) {                 // wait for the exposure delay to complete (if there is one)
            error = this->wait_for_exposure();
            if ( error != NO_ERROR ) {
              logwrite( function, "ERROR: waiting for pre-exposure" );
              this->cleanup_memory();
              return error;
            }
          }

          error = this->wait_for_readout();                             // Wait for the readout into frame buffer,
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: waiting for pre-exposure readout" );
            this->cleanup_memory();
            return error;
          }

          continue;
        }  // end wait for pre-exposures

        // Erase the extensions keys database
        //
        this->extkeys.erasedb();

        // When looping over multiple read/writes where a FITS file has to be
        // opened for each frame (non-mex) then do that in a separate thread
        // because this can take some time.
        //
        if ( !this->camera.mex() || this->camera.mexamps() ) {
          this->openfits_error.store( false, std::memory_order_seq_cst );
          std::thread( std::ref( Archon::Interface::dothread_openfits ), this ).detach();
        }

        if (this->camera.writekeys_when=="after") this->copy_keydb();   // copy the ACF and userkeys database into camera_info

        {
        char *ptr_image;
        try {
          ptr_image = this->image_ring.at(this->ringcount);
#ifdef LOGLEVEL_DEBUG
          message.str(""); message << "[DEBUG] this->image_ring[" << this->ringcount << "] = " << std::hex << (void*)this->image_ring.at(this->ringcount)
                                   << " ptr_image=" << std::hex << (void*)ptr_image;
          logwrite(function, message.str());
#endif
        }
        catch ( std::out_of_range & ) {
          message.str(""); message << "ringcount " << this->ringcount
                                   << " out of range addressing image_ring.size=" << this->image_ring.size();
          this->camera.log_error( function, message.str() );
          this->cleanup_memory();
          return(ERROR);
        }

        // Read each frame into the image buffer pointed to by ptr_image.
        // For data cubes this will loop over cubedepth and all frames go into the same buffer.
        // For single-frame reads, cubedepth=1 so this happens only once.
        //
        uint64_t ts0=0, dts=0;
        int slicecounter;
        switch ( this->camera_info.sampmode ) {
          case SAMPMODE_SINGLE:
            slicecounter = 2;
            break;
          case SAMPMODE_RXRV:
            slicecounter = 1;
            break;
          default:
            slicecounter = this->camera_info.cubedepth;
            break;
        }

        // Loop over the number of slices in this datacube
        //
        for ( int slice=0; !this->camera.is_aborted() && slice < slicecounter; slice++ ) {
          message.str(""); message << "waiting for ";
          if ( this->camera_info.sampmode == SAMPMODE_SINGLE && slice==0 ) {
            message << "first frame (discarded)";
	  }
          else if ( this->camera_info.sampmode == SAMPMODE_SINGLE && slice==1 ) {
            message << "slice 1 of 1";
          }
          else {
            message << "slice " << std::dec << slice+1 << " of " << slicecounter;
          }
          logwrite( function, message.str() );

          error = this->wait_for_readout();                             // Wait for the readout into Archon frame buffer,

          if ( this->camera_info.sampmode == SAMPMODE_SINGLE && slice == 0 ) {
            logwrite( function, "[SAMPMODE_SINGLE] ----- waiting for exposure delay -----" );
            this->last_frame_timer = this->frame.buftimestamp[this->frame.index];
            error=wait_for_exposure();
            if ( error != NO_ERROR ) { logwrite( function, "ERROR" ); return error; }
            continue;
          }

          this->camera_info.stop_time = get_timestamp();                // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
          this->cds_info.stop_time = get_timestamp();                   // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss

          if ( slice==0 ) ts0 = this->frame.buftimestamp[this->frame.index];  // retain the BUFnTIMESTAMP of the first frame
          dts = (this->frame.buftimestamp[this->frame.index]-ts0);      // delta time stamp is change in BUFnTIMESTAMP since the first frame

          if ( this->camera_info.sampmode == SAMPMODE_SINGLE ) dts=0;   // no delta timestamp for single

	  // At the halfway point, use this dts to add a header keyword for TRUITIME
	  //
          if ( ( slicecounter%2 == 0 ) && ( slice == slicecounter/2 ) ) {
            double truitime = static_cast<double>(dts)/100000000.0;
            std::stringstream truitimestr;
            truitimestr << truitime;
            this->cds_info.systemkeys.addkey( "TRUITIME", truitime, "True integration time in seconds (calculated)", 3 );  // new FITS engine will pick this up
            this->camera_info.systemkeys.addkey( "TRUITIME", truitime, "True integration time in seconds (calculated)", 3 );  // new FITS engine will pick this up
//          this->xcds_file.add_key( true, "TRUITIME", "DOUBLE", truitimestr.str(), "True integration time in seconds (calculated)" );
//          this->xfits_file.add_key( true, "TRUITIME", "DOUBLE", truitimestr.str(), "True integration time in seconds (calculated)" );
          }

          // Add Archon TIMESTAMP for this frame buffer to the extkeys database.
          // This keyword database is erased with each exposure and will be written
          // only for multi-extension files, which means that camera_info.ismex must be true.
          //
          int slice_ts = ( this->camera_info.sampmode==SAMPMODE_SINGLE ? slice : slice+1 );
          message.str(""); message << "TS" << slice_ts << "=" << std::dec << std::fixed << std::setprecision(0)
                                   << this->frame.buftimestamp[this->frame.index]
                                   << "// Archon timestamp for slice " << slice_ts << " in 10ns";
          this->extkeys.addkey( message.str() );

          message.str(""); message << "DTS" << slice_ts << "=" << std::dec << dts
                                   << "// Archon delta TS slice " << slice_ts << " in 10ns";
          this->extkeys.addkey( message.str() );

          message.str(""); message << "NSLICE=" << slice_ts << "// slice number";
          this->extkeys.addkey( message.str() );

          // close FITS object on error
          //
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR waiting for readout: closing FITS file" );
//          this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info );
            this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
            if (this->__fits_file) this->__fits_file->complete();
            if ( this->camera_info.iscds ) {
//            this->xcds_file.close_file(  (this->camera.writekeys_when=="after"?true:false), this->cds_info );
              this->cds_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
              if (this->__file_cds) this->__file_cds->complete();
            }
            this->cleanup_memory();
            return error;
          }

          // If this ring buffer is already locked for writing then there is a problem
          //
          if ( (*this->ringlock.at( this->ringcount )).load( std::memory_order_seq_cst ) ) {
            message.str(""); message << "RING BUFFER OVERFLOW: ring buffer " << this->ringcount << " is already locked for writing";
            this->camera.log_error( function, message.str() );
            this->cleanup_memory();
            return ERROR;
          }

          (*this->ringlock.at( this->ringcount )).store(true, std::memory_order_seq_cst);   // mark this ring buffer as locked while reading data into it
          error = this->read_frame( Camera::FRAME_IMAGE, ptr_image, this->ringcount );      // read image frame buffer to host, no write
          (*this->ringlock.at( this->ringcount )).store(false, std::memory_order_seq_cst);  // clear the ring buffer lock flag

          // close FITS object on error
          //
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR reading frame buffer: closing FITS file" );
//          this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info );
            this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
            if (this->__fits_file) this->__fits_file->complete();
            if ( this->camera_info.iscds ) {
//            this->xcds_file.close_file(  (this->camera.writekeys_when=="after"?true:false), this->cds_info );
//            this->cds_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
              if (this->__file_cds) this->__file_cds->complete();
            }
            this->cleanup_memory();
            return error;
          }

          message.str(""); message << "NSLICE:";
          if ( this->camera_info.sampmode == SAMPMODE_SINGLE ) message << slice; else message << slice+1;
          this->camera.async.enqueue( message.str() );
          logwrite( function, message.str() );

#ifdef LOGLEVEL_DEBUG
          message.str(""); message << "[DEBUG] sampmode=" << this->camera_info.sampmode << " slice=" << slice << " cubedepth=" << this->camera_info.cubedepth;
          logwrite( function, message.str() );
#endif
          switch( this->camera_info.sampmode ) {
            case SAMPMODE_UTR:
              if ( (slice+1) < this->camera_info.cubedepth ) {
                logwrite( function, "[SAMPMODE_UTR] ----- waiting for exposure delay -----" );
                this->last_frame_timer = this->frame.buftimestamp[this->frame.index];
                error=wait_for_exposure();
                if ( error != NO_ERROR ) { logwrite( function, "ERROR" ); return error; }
              }
              break;
            case SAMPMODE_CDS:
            case SAMPMODE_MCDS:
              if ( (slice+1) == this->camera_info.cubedepth/2 ) {
                logwrite( function, "[SAMPMODE_M/CDS] ----- waiting for exposure delay -----" );
                this->last_frame_timer = this->frame.buftimestamp[this->frame.index];
                error=wait_for_exposure();
                if ( error != NO_ERROR ) { logwrite( function, "ERROR" ); return error; }
              }
              break;
            default:
              logwrite( function, "----- no exposure delay -----" );
              break;
          }
        }
        }

        // @todo check for cases not mex
	//
        if ( this->camera.writekeys_when=="before" ) this->copy_keydb();  // copy the ACF and userkeys database into camera_info

        if ( ! this->camera.is_aborted() && camera.mex() ) {              // NB. all nirc2 sample modes set mex true
#ifdef LOGLEVEL_DEBUG
          message.str(""); message << "[DEBUG] spawning threads to deinterlace and write ringcount " << this->ringcount;
          logwrite( function, message.str() );
#endif
          // Clear the deinterlaced flag for this ring buffer,
          // then spawn two threads, one to deinterlace and another to write the FITS file.
          // The fits writing thread will block until the deinterlacing thread signals completion.
          //
          this->ringbuf_deinterlaced.at( this->ringcount ) = false;
          std::thread( std::ref( Archon::Interface::dothread_start_deinterlace ), this, this->ringcount ).detach();
          std::thread( std::ref( Archon::Interface::dothread_writeframe ), this, this->ringcount ).detach();
        }
        else if ( ! this->camera.is_aborted() ) {
          // After reading the Archon frame buffers, or
          // after reading all of the multiple frame buffers for a data cube,
          // deinterlace and then write the frame to FITS file.
          //
          switch ( this->camera_info.datatype ) {
            case USHORT_IMG: {
#ifdef LOGLEVEL_DEBUG
              logwrite( function, "[DEBUG] this->camera_info.datatype = USHORT_IMG" );
#endif
              this->deinterlace( (uint16_t*)this->image_ring[this->ringcount],
                                 (uint16_t*)this->work_ring[this->ringcount],
                                 (uint16_t*)this->cds_ring[this->ringcount],
                                 this->ringcount
                               );
              break;
            }
            case SHORT_IMG: {
#ifdef LOGLEVEL_DEBUG
              logwrite( function, "[DEBUG] this->camera_info.datatype = SHORT_IMG" );
#endif
              this->deinterlace( (int16_t *)this->image_ring[this->ringcount], (int16_t *)this->work_ring[this->ringcount], (int16_t*)this->cds_ring[this->ringcount], this->ringcount );
              break;
            }
            case FLOAT_IMG: {
#ifdef LOGLEVEL_DEBUG
              logwrite( function, "[DEBUG] this->camera_info.datatype = FLOAT_IMG" );
#endif
              this->deinterlace( (uint32_t *)this->image_data, (uint32_t *)this->work_ring[this->ringcount], (uint32_t*)this->cds_ring[this->ringcount], this->ringcount );
              break;
            }
            default:
              message.str(""); message << "unknown datatype " << this->camera_info.datatype;
              this->camera.log_error( function, message.str() );
              this->cleanup_memory();
              return ERROR;
              break;
          }
          error = this->write_frame( this->ringcount );                   // write image frame
        }
        else if ( this->camera.is_aborted() ) {
          ++this->write_frame_count;
          ++this->deinterlace_count;
          logwrite( function, "skipping deinterlacing due to abort" );
        }

        // For non-sequence multiple exposures, including mexamps, close the fits file here
        //
        if ( !this->camera.mex() || this->camera.mexamps() ) {          // Error or not, close the file.
#ifdef LOGLEVEL_DEBUG
          logwrite( function, "[DEBUG] closing fits file (1)" );
#endif
          this->camera_info.exposure_aborted = this->camera.is_aborted();
          this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
//        this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info ); // close the file when not using multi-extensions
          if (this->__fits_file) this->__fits_file->complete();
          this->camera.increment_imnum();                           // increment image_num when fitsnaming == "number"

          // ASYNC status message on completion of each file
          //
          message.str(""); message << "FILE:" << this->camera_info.fits_name << " COMPLETE";
          this->camera.async.enqueue( message.str() );
          logwrite( function, message.str() );
        }

        this->inc_ringcount();

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] exposures remaining in sequence: " << std::dec << nseq
                                 << " incremented ringcount to " << this->ringcount;
        logwrite( function, message.str() );
#endif
        if (error != NO_ERROR) break;                               // should be impossible but don't try additional sequences if there were errors

        // prepare and broadcast the appropriate message tag
        //
        message.str("");
        switch( this->camera_info.sampmode ) {
          case Archon::SAMPMODE_SINGLE:
          case Archon::SAMPMODE_CDS:
          case Archon::SAMPMODE_MCDS:
            message << "NCOADD:" << this->camera_info.ncoadd;
            break;
          case Archon::SAMPMODE_UTR:
            message << "NRAMP:" << this->camera_info.ncoadd;
            break;
          case Archon::SAMPMODE_RXV:
          case Archon::SAMPMODE_RXRV:
            message << "NFRAME:" << this->camera_info.ncoadd;
            break;
        }
        this->camera.async.enqueue( message.str() );

      }  // end of sequence loop, while (nseq-- > 0)

//    if ( this->camera.is_aborted() ) {
//      error = this->abort_archon();
//      if ( error != NO_ERROR ) {
//        logwrite( function, "ERROR aborting exposure" );
//        this->cleanup_memory();
//        return( error );
//      }
//      logwrite(function, "Archon exposure aborted");
//    }

    }
/***** not used for NIRC2:
    else if ( mode == "RAW") {
      error = this->get_frame_status();                             // Get the current frame buffer status
      if (error != NO_ERROR) {
        logwrite( function, "ERROR: unable to get frame status" );
        this->cleanup_memory();
        return(ERROR);
      }
      error = this->camera.get_fitsname( this->camera_info.fits_name ); // Assemble the FITS filename
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: couldn't validate fits filename" );
        this->cleanup_memory();
        return( error );
      }
      this->camera_info.systemkeys.keydb = this->systemkeys.keydb;  // copy the systemkeys database into camera_info

      this->add_filename_key();                                     // add filename to system keys database

      this->copy_keydb();                                           // copy the ACF and userkeys databases into camera_info

      this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);

      error = this->xfits_file.open_file( (this->camera.writekeys_when=="before"?true:false), this->camera_info );
      if ( error != NO_ERROR ) {
        this->camera.log_error( function, "couldn't open fits file" );
        this->cleanup_memory();
        return( error );
      }
      error = read_frame();                    // For raw mode just read (and write) immediately
      this->camera_info.exposure_aborted = this->camera.is_aborted();
      this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info );
      this->camera.increment_imnum();          // increment image_num when fitsnaming == "number"
    }
*****/

    // for multi-exposure (non-mexamps) multi-extension files, close the FITS file now that they've all been written
    //
    if ( this->camera.mex() && !this->camera.mexamps() ) {

      // Wait for all of the extensions to have been written
      //
      logwrite( function, "waiting for all frames to be deinterlaced and written" );
      int wfc = this->write_frame_count.load( std::memory_order_seq_cst );
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] write_frame_count=" << wfc << " nseq=" << this->camera_info.nseq;
      logwrite( function, message.str() );
#endif
      while ( wfc < this->camera_info.nseq ) {
        wfc = this->write_frame_count.load( std::memory_order_seq_cst );
//DDSH  if ( this->camera.is_aborted() ) {  // TODO check this
//DDSH    write( function, "aborted, no longer waiting for frames" );
//DDSH    break;
//DDSH  }
      }

      // Make sure to notify the deinterlacing threads so that they stop in case of an abort
      //
      if ( this->camera.is_aborted() ) {
        logwrite( function, "leaving early due to abort" );
        this->deinter_cv.notify_all();
      }
      else {
        logwrite( function, "all frames deinterlaced and written" );
      }

#ifdef LOGLEVEL_DEBUG
      logwrite( function, "[DEBUG] closing fits file (2)" );
#endif
      this->camera_info.exposure_aborted = this->camera.is_aborted();
      // *** THIS IS NORMAL CLOSE FOR NIRC2 ***
//    this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info );
      this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
      if (this->__fits_file) this->__fits_file->complete();
//    if ( this->camera_info.iscds ) this->xcds_file.close_file(  (this->camera.writekeys_when=="after"?true:false), this->cds_info    );
      this->camera.increment_imnum();          // increment image_num when fitsnaming == "number"

      // ASYNC status message on completion of each file
      //
      message.str(""); message << "RAWFILE:" << this->camera_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
      this->camera.async.enqueue( message.str() );
      error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );

      if ( this->camera_info.iscds ) {
        message.str(""); message << "FILE:" << this->cds_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
        this->camera.async.enqueue( message.str() );
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }
    }

    // remember the mexamps setting used for the last completed exposure
    // TODO revisit once region-of-interest is implemented
    //
    this->lastmexamps = this->camera.mexamps();

    this->cleanup_memory();
    debug( "DO_EXPOSE_EXIT" );
    return (error);
  }
  /***** Archon::Interface::do_expose *****************************************/


  /**************** Archon::Interface::wait_for_exposure **********************/
  /**
   * @fn     wait_for_exposure
   * @brief  creates a wait until the exposure delay has completed
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This is not the actual exposure delay, nor does it accurately time the exposure
   * delay. This function merely creates a reasonably accurate wait on the host to
   * allow time for the Archon to complete its exposure delay. This is done by using
   * the exposure time given to the Archon and by using the Archon's internal timer,
   * which is queried here. There is no sense in polling the Archon's timer for the
   * entire exposure time, so this function waits internally for all but the last 1s
   * of the exposure time, then only starts polling the Archon for the remaining time.
   *
   * A prediction is made of what the Archon's timer will be at the end, in order
   * to provide an estimate of completion.
   *
   */
  long Interface::wait_for_exposure() {
    debug( "WAIT_FOR_EXPOSURE_ENTRY" );
    const std::string function("Archon::Interface::wait_for_exposure");
    std::stringstream message;
    long error = NO_ERROR;

    int exposure_timeout_time;  // Time to wait for the exposure delay to time out
    uint64_t timer, increment=0;

    // "exposure_delay" is the amount of time that the Archon is told to delay.
    // This is not "exposure_time" which is the total exposure time, exposure_delay + readouttime.
    //
    int32_t exposure_delay = this->camera_info.exposure_delay;

    // waittime is 1 second less than the exposure time,
    // or 0 if exposure time is <= 1 sec.
    //
    double waittime = ( exposure_delay / this->camera_info.exposure_factor ) - 1;
    waittime = ( waittime < 0 ? 0 : waittime );

    // Wait, (don't sleep) for the above waittime.
    // All that is happening here is a wait -- there is no Archon polling going on here.
    //
    double start_time = get_clock_time();  // get the current clock time from host (in seconds)
    double now = start_time;

    // Prediction is the predicted finish_timer, used to compute exposure time progress,
    // and is computed as last_frame_timer + exposure_delay in Archon ticks.
    // Each Archon tick is 10 nsec (1e8 sec). Divide by exposure_factor (=1 for sec, =1000 for msec).
    //
    uint64_t prediction   = this->last_frame_timer + this->camera_info.exposure_delay * 100000000 / this->camera_info.exposure_factor;

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] exposure_delay=" << this->camera_info.exposure_delay << " exposure_factor=" << this->camera_info.exposure_factor
                             << " waittime=" << waittime << "s  last_frame_timer=" << this->last_frame_timer << " prediction=" << prediction;
    logwrite( function, message.str() );
#endif

//  std::cerr << "exposure progress: ";
    while ( (now - (waittime + start_time) < 0) && not this->camera.is_aborted() ) {
      std::this_thread::sleep_for( std::chrono::milliseconds(10) );  // sleep 10 msec = 1e6 Archon ticks
      increment += 1000000;
      now = get_clock_time();
      this->camera_info.exposure_progress = (double)increment / (double)(prediction - this->last_frame_timer);
      if (this->camera_info.exposure_progress < 0 || this->camera_info.exposure_progress > 1) this->camera_info.exposure_progress=1;
//    std::cerr << std::setw(3) << (int)(this->camera_info.exposure_progress*100) << "\b\b\b";

      // ASYNC status message reports the elapsed time in the chosen unit
      //
      message.str(""); message << "EXPOSURE:" << (int)(this->camera_info.exposure_delay - (this->camera_info.exposure_progress * this->camera_info.exposure_delay));
      this->camera.async.enqueue( message.str() );
    }

    // Set the time out value. If the exposure time is less than a second, set
    // the timeout to 1 second. Otherwise, set it to the exposure time plus
    // 1 second.
    //
    if ( this->camera_info.exposure_delay / this->camera_info.exposure_factor < 1 ) {
      exposure_timeout_time = 1000; //ms
    }
    else {
      exposure_timeout_time = (this->camera_info.exposure_delay) + 1000;
    }

    // Now start polling the Archon for the last remaining portion of the exposure delay
    //
    bool done = false;
    while (done == false && not this->camera.is_aborted() ) {
      // Poll Archon's internal timer
      //
      if ( (error=this->get_timer(&timer)) == ERROR ) {
//      std::cerr << "\n";
        logwrite( function, "ERROR: could not get Archon timer" );
        break;
      }

      // update progress
      //
      this->camera_info.exposure_progress = (double)(timer - this->last_frame_timer) / (double)(prediction - this->last_frame_timer);
      if (this->camera_info.exposure_progress < 0 || this->camera_info.exposure_progress > 1) this->camera_info.exposure_progress=1;

      // ASYNC status message reports the elapsed time in the chosen unit
      //
      message.str(""); message << "EXPOSURE:" << (int)(this->camera_info.exposure_delay - (this->camera_info.exposure_progress * this->camera_info.exposure_delay));
      this->camera.async.enqueue( message.str() );

//    std::cerr << std::setw(3) << (int)(this->camera_info.exposure_progress*100) << "\b\b\b";  // send to stderr in case anyone is watching

      // Archon timer ticks are in 10 nsec (1e-8) so when comparing to exposure_delay,
      // multiply exposure_delay by 1e8/exposure_factor, where exposure_factor=1 or =1000 for exposure_unit sec or msec.
      //
      if ( (timer - this->last_frame_timer) >= ( this->camera_info.exposure_delay * 100000000 / this->camera_info.exposure_factor ) ) {
        this->finish_timer = timer;
        done  = true;
        break;
      }

      std::this_thread::sleep_for( std::chrono::milliseconds(1) );  // a little pause to slow down the requests to Archon

      // Added protection against infinite loops, probably never will be invoked
      // because an Archon error getting the timer would exit the loop.
      // exposure_timeout_time is in msec and it's a little more than 1 msec to get
      // through this loop. If this is decremented each time through then it should
      // never hit zero before the exposure is finished, unless there is a serious
      // problem.
      //
      if (--exposure_timeout_time < 0) {
//      std::cerr << "\n";
        error = ERROR;
        this->camera.log_error( function, "timeout waiting for exposure" );
        break;
      }
    }  // end while (done == false && not this->camera.is_aborted)

//  std::cerr << "\n";

    if ( this->camera.is_aborted() ) {
      error = this->abort_archon();
      logwrite(function, "exposure aborted");
    }

    debug( "WAIT_FOR_EXPOSURE_EXIT" );
    return( error );
  }
  /**************** Archon::Interface::wait_for_exposure **********************/


  /**************** Archon::Interface::wait_for_readout ***********************/
  /**
   * @fn     wait_for_readout
   * @brief  creates a wait until the next frame buffer is ready
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This function polls the Archon frame status until a new frame is ready.
   *
   */
  long Interface::wait_for_readout() {
    debug( "WAIT_FOR_READOUT_ENTRY frame="+std::to_string(this->lastframe+1) );
    const std::string function("Archon::Interface::wait_for_readout");
    std::stringstream message;
    long error = NO_ERROR;
    int currentframe=this->lastframe;
    int busycount=0;
    bool done = false;

    message.str("");
    message << "waiting for new frame: lastframe=" << this->lastframe << " frame.index=" << this->frame.index;
    logwrite(function, message.str());

    // waittime is 10% over the specified readout time
    // and will be used to keep track of timeout errors
    //
    double waittime;
    try {
      waittime = this->camera.readout_time.at(0) * 1.1;        // this is in msec
    }
    catch(std::out_of_range &) {
      message.str(""); message << "readout time for Archon not found from config file";
      this->camera.log_error( function, message.str() );
      debug( "WAIT_FOR_READOUT_EXIT "+std::to_string(this->lastframe+1)+" ERROR" );
      return(ERROR);
    }

    double clock_now     = get_clock_time();                   // get_clock_time returns seconds
    double clock_timeout = clock_now + waittime/1000.;         // must receive frame by this time

    // Poll frame status until current frame is not the last frame and the buffer is ready to read.
    // The last frame was recorded before the readout was triggered in get_frame().
    //
    while ( done == false && not this->camera.is_aborted() ) {

      std::this_thread::sleep_for( std::chrono::microseconds( 100 ) );  // reduces polling frequency
      error = this->get_frame_status();

      // If Archon is busy then ignore it, keep trying for up to ~ 3 second
      // (30000 attempts, ~100us between attempts)
      //
      if (error == BUSY) {
        if ( ++busycount > 30000 ) {
	  done = true;
	  this->camera.log_error( function, "received BUSY from Archon too many times trying to get frame status" );
	  break;
	}
	else continue;
      }
      else busycount=0;

      if (error == ERROR) {
        done = true;
        logwrite( function, "ERROR: unable to get frame status" );
        break;
      }
      currentframe = this->frame.bufframen[this->frame.index];

      if ( (currentframe != this->lastframe) && (this->frame.bufcomplete[this->frame.index]==1) ) {
        done  = true;
        error = NO_ERROR;
        break;
      }  // end if ( (currentframe != this->lastframe) && (this->frame.bufcomplete[this->frame.index]==1) )

      // If the frame isn't done by the predicted time then
      // enough time has passed to trigger a timeout error.
      //
      if (clock_now > clock_timeout) {
        done = true;
        error = ERROR;
        message.str(""); message << "timeout waiting for new frame exceeded " << waittime << ". lastframe = " << this->lastframe;
        this->camera.log_error( function, message.str() );
        break;
      }
      clock_now = get_clock_time();

      // ASYNC status message reports the number of lines read so far,
      // which is buflines not from this->frame.index but from the NEXT index...
      // Only enqueue this message if the buffer is actively being written to, which
      // is when wbuf is the next_index (must subtract 1 from wbuf because it is {1:3}
      // while index is {0:2}).
      //
      if ( this->frame.next_index == this->frame.wbuf-1 ) {
        message.str(""); message << "LINECOUNT:" << this->frame.buflines[ this->frame.next_index ];
        this->camera.async.enqueue( message.str() );
      }
#ifdef LOGLEVEL_DEBUG
      message << " [DEBUG] ";
      message << " index=" << this->frame.index << " next_index=" << this->frame.next_index << " | ";
      for ( int i=0; i < Archon::nbufs; i++ ) { message << " " << this->frame.buflines[ i ]; }
#endif

    } // end while (done == false && not this->camera.is_aborted)

    // After exiting while loop, one update to ensure accurate ASYNC message
    // reporting of LINECOUNT.
    //
    if ( error == NO_ERROR ) {
      error = this->get_frame_status();
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: unable to get frame status" );
      debug( "WAIT_FOR_READOUT_EXIT ERROR" );
        return error;
      }
      message.str(""); message << "LINECOUNT:" << this->frame.buflines[ this->frame.index ];
      this->camera.async.enqueue( message.str() );
    }

    if ( error != NO_ERROR ) {
      this->camera.log_error( function, "waiting for readout" );
      debug( "WAIT_FOR_READOUT_EXIT ERROR" );
      return error;
    }

#ifdef LOGLEVEL_DEBUG
    message.str(""); 
    message << "[DEBUG] lastframe=" << this->lastframe 
            << " currentframe=" << currentframe 
            << " bufcomplete=" << this->frame.bufcomplete[this->frame.index]
            << " timestamp=" << this->frame.buftimestamp[this->frame.index];
    logwrite(function, message.str());
#endif
    this->lastframe = currentframe;

    // On success, write the value to the log and return
    //
    if ( ! this->camera.is_aborted() ) {
      message.str("");
      message << "received currentframe: " << currentframe;
      logwrite(function, message.str());
      debug( "WAIT_FOR_READOUT_EXIT frame="+std::to_string(currentframe) );
      return NO_ERROR;
    }
    // If the wait was stopped, log a message and return NO_ERROR
    //
    else {
      logwrite(function, "wait for readout stopped by external signal");
      this->abort_archon();
      debug( "WAIT_FOR_READOUT_EXIT" );
      return NO_ERROR;
    }
  }
  /**************** Archon::Interface::wait_for_readout ***********************/


  /**************** Archon::Interface::get_parameter **************************/
  /**
   * @fn     get_parameter
   * @brief  get parameter using read_parameter()
   * @param  string
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::get_parameter(std::string parameter, std::string &retstring) {
    const std::string function("Archon::Interface::get_parameter");

    return this->read_parameter(parameter, retstring);
  }
  /**************** Archon::Interface::get_parameter **************************/


  /**************** Archon::Interface::set_parameter **************************/
  /**
   * @fn     set_parameter
   * @brief  set an Archon parameter
   * @param  string
   * @return ERROR or NO_ERROR
   *
   * This function calls "prep_parameter()" and "load_parameter()"
   *
   */
  long Interface::set_parameter( std::string parameter, long value ) {
    std::stringstream paramstring;
    paramstring << parameter << " " << value;
    return( set_parameter( paramstring.str() ) );
  }
  long Interface::set_parameter(std::string parameter) {
    const std::string function("Archon::Interface::set_parameter");
    std::stringstream message;
    long ret=ERROR;
    std::vector<std::string> tokens;

    Tokenize(parameter, tokens, " ");

    // Check that the input string contains two tokens, "paramname value"
    //
    if (tokens.size() != 2) {
      message.str(""); message << "param expected 2 arguments (paramname and value) but got \"" << parameter << "\"";
      this->camera.log_error( function, message.str() );
      ret=ERROR;
    }
    else {
      // Correct number of tokens so send PREPPARAM, LOADPARAM here
      //
      ret = this->prep_parameter(tokens[0], tokens[1]);
      if (ret == NO_ERROR) ret = this->load_parameter(tokens[0], tokens[1]);

      // Change the value in the parammap
      //
      if ( this->parammap.find(tokens[0]) == this->parammap.end() ) {
        ret = ERROR;
        message.str(""); message << "parameter \"" << tokens[0] << "\" not found in parammap";
        this->camera.log_error( function, message.str() );
      }
      else {
        this->parammap[tokens[0]].value = tokens[1];
      }
    }

    return(ret);
  }
  /**************** Archon::Interface::set_parameter **************************/


  /***** Archon::Interface::exptime *******************************************/
  /**
   * @brief      set/get the exposure time
   * @param[in]  exptime_in  input int value requested exposure time
   * @return     ERROR or NO_ERROR
   *
   * This function is overloaded.
   * This version accepts an integer and does not return the current exposure time.
   *
   */
  long Interface::exptime( int32_t exptime_in ) {
    const std::string function("Archon::Interface::exptime");
    std::stringstream message;
    std::string dontcare;
    std::string exptime;
    try { exptime = std::to_string( exptime_in ); }
    catch ( std::bad_alloc & ) {
      message.str(""); message << "converting exposure time integer value \"" << exptime_in << "\" to string";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }
    return this->exptime( exptime, dontcare );
  }
  /***** Archon::Interface::exptime *******************************************/


  /***** Archon::Interface::exptime *******************************************/
  /**
   * @brief      set/get the exposure time
   * @param[in]  exptime_in  input string contains requested exposure time
   * @return     ERROR or NO_ERROR
   *
   * This function is overloaded.
   * This version does not return the current exposure time.
   *
   */
  long Interface::exptime(std::string exptime_in) {
    std::string dontcare;
    return this->exptime( exptime_in, dontcare );
  }
  /***** Archon::Interface::exptime *******************************************/


  /***** Archon::Interface::exptime *******************************************/
  /**
   * @brief      set/get the exposure time
   * @param[in]  exptime_in  input string contains requested exposure time
   * @param[out] retstring   return string contains the current exposure time
   * @return     ERROR or NO_ERROR
   *
   * The class variable exposure_time is the total exposure time, taking into
   * account any frame readout time (so if you ask for too small of an exposure
   * time then you might not get it).
   *
   * This function calls "set_parameter()" and "get_parameter()" using
   * the "exptime" parameter (which must already be defined in the ACF file).
   *
   * This function is overloaded.
   *
   */
  long Interface::exptime(std::string exptime_in, std::string &retstring) {
    const std::string function("Archon::Interface::exptime");
    std::stringstream message;
    long ret=NO_ERROR;
    int32_t requested_exptime = -1;  // this is the user-requested total exposure time, extracted from exptime_in
    int32_t exp_delay = 0;           // this is the exposure delay that will be sent to the Archon

    // Cannot start another exposure while currently exposing
    //
    if ( this->camera.is_exposing() ) {
      this->camera.log_error( function, "cannot change exposure time while exposure in progress" );
      return ERROR;
    }

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] exptime_in=" << exptime_in; logwrite( function, message.str() );
#endif

    if ( !exptime_in.empty() ) {
      // Convert to integer to check the value
      //
      try {
        requested_exptime = std::stoi( exptime_in );
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "converting exposure time: " << exptime_in << " to integer";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "requested exposure time: " << exptime_in << " outside integer range";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }

      // Archon allows only 20 bit parameters
      //
      if ( requested_exptime < 0 || requested_exptime > 0xFFFFF ) {
        message.str(""); message << "requested exposure time: " << exptime_in << " out of range {0:1048575}";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }

      // Save the actual requested exptime to the class.
      // This is stored so that if there is a change in ROI we can always try again to meet
      // the original exptime request which might not have been met due to geometry/sampmode.
      //
      this->camera_info.requested_exptime = requested_exptime;

      // Check against the readout time, which is the minimum exposure time
      //
      if ( this->camera_info.readouttime > requested_exptime ) {        // When requested exposure time is less than the minimum,
        exp_delay = 0;                                                  // then there will be no added exposure delay,
        requested_exptime = this->camera_info.readouttime;              // and make it as if they requested the correct exptime.
      }
      else {
        exp_delay = requested_exptime - this->camera_info.readouttime;  // Otherwise the Archon's exposure delay will be the difference.
      }

      // Now that the value is OK set the parameter on the Archon
      //
      std::stringstream cmd;
      try { cmd << "exptime " << std::to_string( exp_delay ); }
      catch ( std::bad_alloc & ) {
        message.str(""); message << "converting exposure delay integer value \"" << exp_delay << "\" to string";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      ret = this->set_parameter( cmd.str() );

      // If parameter was set OK then save the new values to the class
      //
      if ( ret==NO_ERROR ) {
        this->camera_info.exposure_time  = requested_exptime;
        this->camera_info.exposure_delay = exp_delay;
      }
    }

    // broadcast the exposure delay and exposure time
    //
    message.str(""); message << "EXPDELAY:" << this->camera_info.exposure_delay << ( this->is_longexposure ? " sec" : " msec" );
    this->camera.async.enqueue( message.str() );
    message.str(""); message << "EXPTIME:" << this->camera_info.exposure_time << ( this->is_longexposure ? " sec" : " msec" );
    this->camera.async.enqueue( message.str() );

    // prepare the return value
    //
    message.str(""); message << this->camera_info.exposure_time << ( this->is_longexposure ? " sec" : " msec" );
    retstring = message.str();

    message.str(""); message << "exposure time is " << retstring;
    logwrite(function, message.str());

    debug( "EXPTIME "+retstring );
    return(ret);
  }
  /***** Archon::Interface::exptime *******************************************/


  /***** Archon::Interface::shutter *******************************************/
  /**
   * @brief  set or get the shutter enable state
   * @param  std::string shutter_in
   * @param  std::string& shutter_out
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::shutter(std::string shutter_in, std::string& shutter_out) {
    const std::string function("Archon::Interface::shutter");
    std::stringstream message;
    long error = NO_ERROR;
    int level=0, force=0;  // trigout level and force for activate

    if ( this->shutenableparam.empty() ) {
      this->camera.log_error( function, "SHUTENABLE_PARAM is not defined in configuration file" );
      return( ERROR );
    }

    if ( !shutter_in.empty() ) {
      try {
        bool shutten;          // shutter enable state read from command
        bool ability=false;    // are we going to change the ability (enable/disable)?
        bool activate=false;   // are we going to activate the shutter (open/close)?
        std::string activate_str;
        bool dontcare;
        std::transform( shutter_in.begin(), shutter_in.end(), shutter_in.begin(), ::tolower );  // make lowercase
        if ( shutter_in == "disable" ) {
          ability = true;
          shutten = false;
	}
        else
        if ( shutter_in == "enable"  ) {
          ability = true;
          shutten = true;
	}
        else
        if ( shutter_in == "open"  ) {
          activate = true;
          force = 1;
          level = 1;
          activate_str = "open";
	}
        else
        if ( shutter_in == "close"  ) {
          activate = true;
          force = 1;
          level = 0;
          activate_str = "closed";
	}
        else
        if ( shutter_in == "reset"  ) {
          activate = true;
          force = 0;
          level = 0;
          activate_str = "";
          // shutter back to normal operation;
          // shutter force level keyword has no context so remove it from the system keys db
          //
          this->systemkeys.EraseKeys( "SHUTFORC" );
	}
        else {
          message.str(""); message << shutter_in << " is invalid. Expecting { enable | disable | open | close | reset }";
          this->camera.log_error( function, message.str() );
          error = ERROR;
        }
        // if changing the ability (enable/disable) then do that now
	//
        if ( error == NO_ERROR && ability ) {
          std::stringstream cmd;
          cmd << this->shutenableparam << " " << ( shutten ? this->shutenable_enable : this->shutenable_disable );
          error = this->set_parameter( cmd.str() );

          // If parameter was set OK then save the new value
          //
          if ( error == NO_ERROR ) this->camera_info.shutterenable = shutten;
        }
        // if changing the activation (open/close/reset) then do that now
	//
        if ( error == NO_ERROR && activate ) {
          if ( this->configmap.find( "TRIGOUTFORCE" ) != this->configmap.end() ) {
            error = this->write_config_key( "TRIGOUTFORCE", force, dontcare );
	  }
          else {
            this->camera.log_error( function, "TRIGOUTFORCE not found in configmap" );
            error = ERROR;
          }
          if ( this->configmap.find( "TRIGOUTLEVEL" ) != this->configmap.end() ) {
            if ( error == NO_ERROR) error = this->write_config_key( "TRIGOUTLEVEL", level, dontcare );
	  }
          else {
            this->camera.log_error( function, "TRIGOUTLEVEL not found in configmap" );
            error = ERROR;
          }
          if ( error == NO_ERROR ) error = this->archon_cmd( APPLYSYSTEM );
          if ( error == NO_ERROR ) { this->camera_info.shutteractivate = activate_str; }
	}
      }
      catch (...) {
        message.str(""); message << "converting requested shutter state: " << shutter_in << " to lowercase";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }

    // set the return value and report the state now, either setting or getting
    //
    shutter_out = this->camera_info.shutterenable ? "enabled" : "disabled";

    // if the shutteractivate string is not empty then use it for the return string, instead
    //
    if ( !this->camera_info.shutteractivate.empty() ) shutter_out = this->camera_info.shutteractivate;

    message.str("");
    message << "shutter is " << shutter_out;
    logwrite( function, message.str() );

    // If the shutter was forced open or closed then add that to the system keys db
    //
    if ( force ) {
      message.str(""); message << "SHUTFORC=" << level << "// shutter force level";
      this->systemkeys.addkey( message.str() );
    }

    // Add the shutter enable keyword to the system keys db
    //
    message.str(""); message << "SHUTTEN=" << ( this->camera_info.shutterenable ? "T" : "F" ) << "// shutter was enabled";
    this->systemkeys.addkey( message.str() );

    return error;
  }
  /***** Archon::Interface::shutter *******************************************/


  /**************** Archon::Interface::hdrshift *******************************/
  /**
   * @fn     hdrshift
   * @brief  set/get number of hdrshift bits
   * @param  bits_in
   * @param  &bits_out
   * @return ERROR or NO_ERROR
   *
   * This function sets (or gets) this->n_hdrshift.
   *
   * In HDR mode (i.e. SAMPLEMODE=1, 32 bits per pixel) this->write_frame()
   * will right-shift the Archon data buffer by this->n_hdrshift.
   *
   */
  long Interface::hdrshift(std::string bits_in, std::string &bits_out) {
    const std::string function("Archon::Interface::hdrshift");
    std::stringstream message;
    int hdrshift_req=-1;

    // If something is passed then try to use it to set the number of hdrshifts
    //
    if ( !bits_in.empty() ) {
      try {
        hdrshift_req = std::stoi( bits_in );
      }
      catch ( std::invalid_argument & ) {
        message.str(""); message << "converting hdrshift: " << bits_in << " to integer";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      catch ( std::out_of_range & ) {
        message.str(""); message << "hdrshift: " << bits_in << " is outside integer range";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }

    if ( hdrshift_req < 0 || hdrshift_req > 31 ) {
      this->camera.log_error( function, "hdrshift outside range {0:31}" );
      return( ERROR );
    }
    else this->n_hdrshift = hdrshift_req;

    // error or not, the number of bits reported now will be whatever was last successfully set
    //
    bits_out = std::to_string( this->n_hdrshift );

    // add to system keyword database
    //
    std::stringstream keystr;
    keystr << "HDRSHIFT=" << this->n_hdrshift << "// number of HDR right-shift bits";
    this->systemkeys.addkey( keystr.str() );

    return( NO_ERROR );
  }
  /**************** Archon::Interface::hdrshift *******************************/


  /**************** Archon::Interface::copy_keydb *****************************/
  /**
   * @fn         copy_keydb
   * @brief      copy the ACF and user keyword databases into camera_info
   * @param[in]  none
   * @return     none
   *
   */
  void Interface::copy_keydb() {
    const std::string function("Archon::Interface::copy_keydb");
    std::stringstream message;

    // copy the userkeys database object into camera_info
    //
    this->camera_info.userkeys.keydb = this->userkeys.keydb;
    this->camera_info.extkeys.keydb  = this->extkeys.keydb;

    // copy keys for the cds processed file if needed
    //
    if ( this->camera_info.iscds ) {
      this->cds_info.userkeys.keydb  = this->userkeys.keydb;
      this->cds_info.extkeys.keydb   = this->extkeys.keydb;
      // also must insert this special key just for cds proc
      message.str(""); message << "CDS_OFFS=" << Archon::CDS_OFFS << " // CDS read frame offset";
      this->cds_info.extkeys.addkey( message.str() );
    }

    // add any keys from the ACF file (from modemap[mode].acfkeys) into the
    // camera_info.userkeys object
    //
    std::string mode = this->camera_info.current_observing_mode;
    Common::FitsKeys::fits_key_t::iterator keyit;
    for ( keyit  = this->modemap[mode].acfkeys.keydb.begin();
          keyit != this->modemap[mode].acfkeys.keydb.end();
          keyit++ ) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;

      // and for cds proc
      //
      if ( this->camera_info.iscds ) {
        this->cds_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
        this->cds_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
        this->cds_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
        this->cds_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
      }
    }

#ifdef LOGLEVEL_DEBUG
    logwrite( function, "[DEBUG] copied userkeys db to camera_info" );
#endif

    return;
  }
  /**************** Archon::Interface::copy_keydb *****************************/


  /**************** Archon::Interface::heater *********************************/
  /**
   * @fn     heater
   * @brief  heater control, set/get state, target, PID, ramp
   * @param  args contains various allowable strings (see full descsription)
   * @return ERROR or NO_ERROR
   *
   * valid args format,
   * to set or get the enable state and target for heater A or B on the specified module:
   *   <module> < A | B > [ <on | off> <target> ]
   *   possible args: 2 (get) or 3 or 4
   *
   * to set or get the PID parameters for heater A or B on the specified module:
   *   <module> < A | B > PID [ <p> <i> <d> ]
   *   possible args: 3 (get) or 6 (set)
   *
   * to set or get the ramp and ramprate for heater A or B on the specified module:
   *   <module> < A | B > RAMP [ <on | off> [ramprate] ]
   *   possible args: 3 (get) or 5 (set)
   *
   * to set or get the current limit for heater A or B on the specified module:
   *   <module> < A | B > ILIM [ <value> ]
   *   possible args: 3 (get) or 4 (set)
   *
   * to set or get the input sensor for heater A or B on the specified module:
   *   <module> < A | B > INPUT [ A | B | C ]
   *   possible args: 3 (get) or 4 (set)
   *
   */
  long Interface::heater(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::heater");
    std::stringstream message;
    std::vector<std::string> tokens;
    int module;
    std::string heaterid;                   //!< A|B
    bool readonly=false;
    float pid_p, pid_i, pid_d;              //!< requested PID values
    int ramprate;                           //!< requested ramp rate value
    float target;                           //!< requested heater target value
    std::vector<std::string> heaterconfig;  //!< vector of configuration lines to read or write
    std::vector<std::string> heatervalue;   //!< vector of values associated with config lines (for write)

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return( ERROR );
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    // RAMP requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_RAMP );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_RAMP;
      }
      else {
        message << "requires backplane version " << REV_RAMP << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    Tokenize(args, tokens, " ");

    // At minimum there must be two tokens, <module> A|B
    //
    if ( tokens.size() < 2 ) {
      this->camera.log_error( function, "expected at least two arguments: <module> A|B" );
      return ERROR;
    }

    // As long as there are at least two tokens, get the module and heaterid
    // which will be common to everything.
    //
    try {
      module   = std::stoi( tokens[0] );
      heaterid = tokens[1];
      if ( heaterid != "A" && heaterid != "B" ) {
        message.str(""); message << "invalid heater " << heaterid << ": expected <module> A|B";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
    }
    catch (std::invalid_argument &) {
      message.str(""); message << "converting heater <module> " << tokens[0] << " to integer";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }
    catch (std::out_of_range &) {
      message.str(""); message << "heater <module>: " << tokens[0] << " is outside integer range";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // check that requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      case 5:  // Heater
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "module " << module << " not a heater board";
        this->camera.log_error( function, message.str() );
        return(ERROR);
    }

    // Now that we've passed the basic tests (firmware OK, basic syntax OK, and requested module is a heater)
    // go ahead and set up some needed variables.
    //

    // heater target min/max depends on backplane version
    //
    ret = compare_versions( this->backplaneversion, REV_HEATERTARGET );
    if ( ret == -999 ) {
      message.str(""); message << "comparing backplane version " << this->backplaneversion << " to " << REV_HEATERTARGET;
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }
    else if ( ret == -1 ) {
      this->heater_target_min = -150.0;
      this->heater_target_max =   50.0;
    }
    else {
      this->heater_target_min = -250.0;
      this->heater_target_max =   50.0;
    }

    heaterconfig.clear();
    heatervalue.clear();
    std::stringstream ss;

    // Any one heater command can require reading or writing multiple configuration lines.
    // The code will look at each case and push each heater configuration line that needs
    // to be read or written into a vector (heaterconfig). For those that need to be written,
    // the value to write will be pushed into a separate vector (heatervalue). After these
    // vectors have been built up, the code will loop through each element of the vectors,
    // reading and/or writing each.

    // Start looking at the heater arguments passed, which have been tokenized.
    // There can be only 2, 3, 4, 5, or 6 tokens allowed.

    // If there are exactly two (2) tokens then we have received only:
    // <module> A|B
    // which means we're reading the state and target.
    //
    if ( tokens.size() == 2 ) {        // no args reads ENABLE, TARGET
      readonly = true;
      ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
      ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
    }
    else
    // If there are three (3) tokens then the 3rd must be one of the following:
    // ON | OFF (for ENABLE), <target>, PID, RAMP, ILIM
    //
    if ( tokens.size() == 3 ) {
      if ( tokens[2] == "ON" ) {       // ON
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "1" );
      }
      else
      if ( tokens[2] == "OFF" ) {      // OFF
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "0" );
      }
      else
      if ( tokens[2] == "RAMP" ) {     // RAMP
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
      }
      else
      if ( tokens[2] == "PID" ) {      // PID
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "P"; heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "I"; heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "D"; heaterconfig.push_back( ss.str() );
      }
      else
      if ( tokens[2] == "ILIM" ) {     // ILIM
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "IL"; heaterconfig.push_back( ss.str() );
      }
      else
      if ( tokens[2] == "INPUT" ) {    // INPUT
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "SENSOR"; heaterconfig.push_back( ss.str() );
      }
      else {                           // <target>
        // first check that the requested target is a valid number and within range...
        //
        try {
          target = std::stof( tokens[2] );
         if ( target < this->heater_target_min || target > this->heater_target_max ) {
           message.str(""); message << "requested heater target " << target << " outside range {" 
                                    << this->heater_target_min << ":" << this->heater_target_max << "}";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "converting heater <target>=" << tokens[2] << " to float";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "heater <target>: " << tokens[2] << " outside range of float";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        // ...if target is OK then push into the config, value vectors
        //
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[2] );
      }
    }
    else
    // If there are four (4) tokens then the 4th must be one of the following:
    // <target>               in <module> < A | B > [ <on | off> <target> ]
    // <ramprate> | ON | OFF  in <module> < A | B > RAMP [ <on | off> [ramprate] ]
    // <value>                in <module> < A | B > ILIM [ <value> ]
    // A | B | C              in <module> < A | B > INPUT [ A | B | C ]
    //
    if ( tokens.size() == 4 ) {

      if ( tokens[2] == "ON" ) {       // ON <target>
        // first check that the requested target is a valid number and within range...
        //
        try {
          target = std::stof( tokens[3] );
         if ( target < this->heater_target_min || target > this->heater_target_max ) {
           message.str(""); message << "requested heater target " << target << " outside range {" 
                                    << this->heater_target_min << ":" << this->heater_target_max << "}";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "converting heater <target> " << tokens[3] << " to float";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "heater <target>: " << tokens[3] << " outside range of float";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        // ...if target is OK then push into the config, value vectors
        //
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "1" );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );
      }
      else
      if ( tokens[2] == "RAMP" ) {     // RAMP x

        if ( tokens[3] == "ON" || tokens[3] == "OFF" ) {   // RAMP ON|OFF
          ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
          std::string state = ( tokens[3]=="ON" ? "1" : "0" );
          heatervalue.push_back( state );
        }
        else {                                             // RAMP <ramprate>
          try {
            ramprate = std::stoi( tokens[3] );
            if ( ramprate < 1 || ramprate > 32767 ) {
              message.str(""); message << "heater ramprate " << ramprate << " outside range {1:32767}";
              this->camera.log_error( function, message.str() );
              return( ERROR );
            }
          }
          catch (std::invalid_argument &) {
            message.str(""); message << "converting RAMP <ramprate> " << tokens[3] << " to integer";
            this->camera.log_error( function, message.str() );
            return(ERROR);
          }
          catch (std::out_of_range &) {
            message.str(""); message << "RAMP <ramprate>: " << tokens[3] << " outside range of integer";
            this->camera.log_error( function, message.str() );
            return(ERROR);
          }
          ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
          heatervalue.push_back( tokens[3] );
        }
      }
      else
      if ( tokens[2] == "ILIM" ) {     // ILIM x
        int il_value=0;
        try {
          il_value = std::stoi( tokens[3] );
          if ( il_value < 0 || il_value > 10000 ) {
            message.str(""); message << "heater ilim " << il_value << " outside range {0:10000}";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "converting ILIM <value> " << tokens[3] << " to integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "ILIM <value>: " << tokens[3] << " outside range of integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "IL"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );
      }
      else
      if ( tokens[2] == "INPUT" ) {    // INPUT A|B|C
        std::string sensorid;
        if ( tokens[3] == "A" ) { sensorid = "0"; } else
        if ( tokens[3] == "B" ) { sensorid = "1"; } else
        if ( tokens[3] == "C" ) { sensorid = "2";
          // input C supported only on HeaterX cards
          if ( this->modtype[ module-1 ] != 11 ) {
            message.str(""); message << "sensor C not supported on module " << module << ": HeaterX module required";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }
        else {
          message.str(""); message << "invalid sensor " << tokens.at(3) << ": expected <module> A|B INPUT A|B|C";
          this->camera.log_error( function, message.str() );
          return( ERROR );
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "SENSOR"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( sensorid );
      }
      else {
        message.str(""); message << "expected heater <" << module << "> ON | RAMP for 3rd argument but got " << tokens[2];
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }
    else
    // If there are five (5) tokens then they must be
    // <module> A|B RAMP ON <ramprate>
    //
    if ( tokens.size() == 5 ) {        // RAMP ON <ramprate>
      if ( tokens[2] != "RAMP" && tokens[3] != "ON" ) {
        message.str(""); message << "expected RAMP ON <ramprate> but got"; for (int i=2; i<5; i++) message << " " << tokens[i];
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      else {  // got "<module> A|B RAMP ON" now check that the last (5th) token is a number
        try {
          ramprate = std::stoi( tokens[4] );
          if ( ramprate < 1 || ramprate > 32767 ) {
            message.str(""); message << "heater ramprate " << ramprate << " outside range {1:32767}";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "expected RAMP ON <ramprate> but unable to convert <ramprate>=" << tokens[4] << " to integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "expected RAMP ON <ramprate> but <ramprate>=" << tokens[4] << " outside range of integer";
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
        heatervalue.push_back( "1" );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[4] );
      }
    }
    else
    // If there are six (6) tokens then they must be
    // <module> A|B PID <p> <i> <d>
    //
    if ( tokens.size() == 6 ) {
      if ( tokens[2] != "PID" ) {
        message.str(""); message << "expected PID <p> <i> <d> but got"; for (int i=2; i<6; i++) message << " " << tokens[i];
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      else {  // got "<module> A|B PID <p> <i> <d>" now check that the last 3 tokens are numbers
        // Fractional PID requires a minimum backplane version
        //
        bool fractionalpid_ok = false;
        ret = compare_versions( this->backplaneversion, REV_FRACTIONALPID );
        if ( ret == -999 ) {
          message.str(""); message << "comparing backplane version " << this->backplaneversion << " to " << REV_FRACTIONALPID;
          this->camera.log_error( function, message.str() );
          return( ERROR );
        }
        else if ( ret == -1 ) fractionalpid_ok = false;
        else fractionalpid_ok = true;
        try {
          // If using backplane where fractional PID was not allowed, check for decimal points
          //
          if ( (!fractionalpid_ok) &&
               ( ( tokens[3].find(".") != std::string::npos ) ||
                 ( tokens[4].find(".") != std::string::npos ) ||
                 ( tokens[5].find(".") != std::string::npos ) ) ) {
            fesetround(FE_TONEAREST);  // round to value nearest X. halfway cases rounded away from zero
            tokens[3] = std::to_string( std::lrint( std::stof( tokens[3] ) ) ); // replace token with rounded integer
            tokens[4] = std::to_string( std::lrint( std::stof( tokens[4] ) ) ); // replace token with rounded integer
            tokens[5] = std::to_string( std::lrint( std::stof( tokens[5] ) ) ); // replace token with rounded integer

            message.str(""); message << "NOTICE:fractional heater PID requires backplane version " << REV_FRACTIONALPID << " or newer";
            logwrite( function, message.str() );
            this->camera.async.enqueue( message.str() );
            message.str(""); message << "NOTICE:backplane version " << this->backplaneversion << " detected";
            logwrite( function, message.str() );
            this->camera.async.enqueue( message.str() );
            message.str(""); message << "NOTICE:PIDs converted to: " << tokens[3] << " " << tokens[4] << " " << tokens[5];
            this->camera.async.enqueue( message.str() );
            logwrite( function, message.str() );

          }
          pid_p = std::stof( tokens[3] );
          pid_i = std::stof( tokens[4] );
          pid_d = std::stof( tokens[5] );
          if ( pid_p < 0 || pid_p > 10000 || pid_i < 0 || pid_i > 10000 || pid_d < 0 || pid_d > 10000 ) {
            message.str(""); message << "one or more heater PID values outside range {0:10000}";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }
        catch (std::invalid_argument &) {
          message.str(""); message << "converting one or more heater PID values to numbers:";
          for (int i=3; i<6; i++) message << " " << tokens[i];
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        catch (std::out_of_range &) {
          message.str(""); message << "heater PID exception: one or more values outside range:";
          for (int i=3; i<6; i++) message << " " << tokens[i];
          this->camera.log_error( function, message.str() );
          return(ERROR);
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "P"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "I"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[4] );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "D"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[5] );
      }
    }
    // Otherwise we have an invalid number of tokens
    //
    else {
      message.str(""); message << "received " << tokens.size() << " arguments but expected 2, 3, 4, 5, or 6";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    long error;

    if ( ! readonly ) {

      // For writing, loop through the heaterconfig and heatervalue vectors.
      // They MUST be the same size! This should be impossible.
      //
      if ( heaterconfig.size() != heatervalue.size() ) {
        message.str("");
        message << "BUG DETECTED: heaterconfig (" << heaterconfig.size() 
                << ") - heatervalue (" << heatervalue.size() << ") vector size mismatch";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }

      // Write the configuration lines
      //
      bool changed = false;
      size_t error_count = 0;
      for ( size_t i=0; i < heaterconfig.size(); i++ ) {
        error = this->write_config_key( heaterconfig[i].c_str(), heatervalue[i].c_str(), changed );
        message.str("");
        if ( error != NO_ERROR ) {
          message << "writing configuration " << heaterconfig[i] << "=" << heatervalue[i];
          error_count++;  // this counter will be checked before the APPLYMOD command
        }
        else if ( !changed ) {
          message << "heater configuration: " << heaterconfig[i] << "=" << heatervalue[i] << " unchanged";
        }
        else {
          message << "updated heater configuration: " << heaterconfig[i] << "=" << heatervalue[i];
        }
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }

      // send the APPLMODxx command which parses and applies the configuration for module xx
      // The APPLYMOD is sent even if an error occured writing the config key(s) because
      // it's possible that one of the config keys was written.
      //
      // If error_count is the same as the number of configuration lines, then they all failed
      // to write, in which case do not send APPLYMOD. But if even one config key was written
      // then the APPLYMOD command must be sent.
      //
      if ( error_count == heaterconfig.size() ) {
        return( ERROR );
      }

      std::stringstream applystr;
      applystr << "APPLYMOD"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);

      error = this->archon_cmd( applystr.str() );

      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: applying heater configuration" );
      }
    }

    // Now read the configuration line(s).
    // For multiple lines, concatenate all values into one space-delimited string.

    // loop through the vector of heaterconfig keys,
    // getting the value for each, and putting them into retss
    //
    std::string value;
    std::stringstream retss;
    for ( auto key : heaterconfig ) {

      error = this->get_configmap_value( key, value );

      if ( error != NO_ERROR ) {
        message.str(""); message << "reading heater configuration " << key;
        logwrite( function, message.str() );
        return( error );
      }
      else {
        // If key ends with "ENABLE" or "RAMP"
        // then convert the values 0,1 to OFF,ON, respectively
        //
        if ( key.substr( key.length()-6 ) == "ENABLE" ||
             key.substr( key.length()-4 ) == "RAMP" ) {
          if ( value == "0" ) value = "OFF"; else
          if ( value == "1" ) value = "ON";
          else {
            message.str(""); message << "bad value " << value << " from configuration. expected 0 or 1";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }
        }
        else
        // or if key ends with "SENSOR" then map the value (0,1,2) to the name (A,B,C)
        //
        if ( key.substr( key.length()-6 ) == "SENSOR" ) {
          if ( value == "0" ) value = "A"; else
          if ( value == "1" ) value = "B"; else
          if ( value == "2" ) value = "C"; 
          else {
            message.str(""); message << "bad value " << value << " from configuration. expected 0,1,2";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }
        }
        retss << value << " ";
        message.str(""); message << key << "=" << value;
        logwrite( function, message.str() );
      }
    }
    retstring = retss.str();  // return value to calling function, passed by reference

    return ( error );
  }
  /**************** Archon::Interface::heater *********************************/


  /**************** Archon::Interface::sensor *********************************/
  /**
   * @fn     sensor
   * @brief  set or get temperature sensor current
   * @param  args contains various allowable strings (see full descsription)
   * @return ERROR or NO_ERROR
   *
   * sensor <module> < A | B | C > [ current ]
   *        possible args: 2 (get) or 3 (set)
   *
   * sensor <module> < A | B | C > AVG [ N ]
   *        possible args: 3 (get) or 4 (set)
   *
   * Sets or gets the temperature sensor current <current> for the specified
   * sensor <A|B|C> on the specified module <module>. <module> refers to the
   * (integer) module number. <current> is specified in nano-amps. 
   * This is used only for RTDs
   *
   * When the AVG arg is used then set or get digital averaging
   *
   */
  long Interface::sensor(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::sensor");
    std::stringstream message;
    std::vector<std::string> tokens;
    std::string sensorid;                   //!< A | B | C
    std::stringstream sensorconfig;         //!< configuration line to read or write
    std::string sensorvalue="";             //!< configuration line value
    int module;                             //!< integer module number
    bool readonly=true;                     //!< true is reading, not writing current
    long error;

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return( ERROR );
    }

    // requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_SENSORCURRENT );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_SENSORCURRENT;
      }
      else {
        message << "requires backplane version " << REV_SENSORCURRENT << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    Tokenize( args, tokens, " " );

    // At minimum there must be two tokens, <module> <sensorid>
    //
    if ( tokens.size() < 2 ) {
      this->camera.log_error( function, "expected at least two arguments: <module> A|B" );
      return ERROR;
    }

    // Get the module and sensorid
    //
    try {
      module   = std::stoi( tokens.at(0) );
      sensorid = tokens.at(1);

      if ( sensorid != "A" && sensorid != "B" && sensorid != "C" ) {
        message.str(""); message << "invalid sensor " << sensorid << ": expected <module#> <A|B|C> [ current | AVG [N] ]";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }
    catch ( std::invalid_argument & ) {
      message.str(""); message << "parsing argument: " << args << ": expected <module#> <A|B|C> [ current | AVG [N] ]";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }
    catch ( std::out_of_range & ) {
      message.str(""); message << "argument outside range in " << args << ": expected <module#> <A|B|C> [ current | AVG [N] ]";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    // check that the requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      case 5:  // Heater
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "module " << module << " is not a heater board";
        this->camera.log_error( function, message.str() );
        return( ERROR );
    }

    // input C supported only on HeaterX cards
    if ( sensorid == "C" && this->modtype[ module-1 ] != 11 ) {
      message.str(""); message << "sensor C not supported on module " << module << ": HeaterX module required";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    // Now check the number of tokens to decide how to next proceed...

    // If there are 2 tokens then must be to read the current,
    //  <module> < A | B | C > 
    //
    if ( tokens.size() == 2 ) {
      readonly = true;
      sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "CURRENT";
    }
    else

    // If there are 3 tokens then it is either to write the current,
    //  <module> < A | B | C > current
    // or to read the average,
    //  <module> < A | B | C > AVG
    //
    if ( tokens.size() == 3 ) {
      // if the 3rd arg is AVG then read the average (MODmSENSORxFILTER)
      //
      if ( tokens[2] == "AVG" ) {
        readonly = true;
        sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "FILTER";
      }
      else {
        // if it's not AVG then assume it's a current value and try to convert it to int
        //
        int current_val=-1;
        try {
          current_val = std::stoi( tokens[2] );
        }
        catch ( std::invalid_argument & ) {
          message.str(""); message << "parsing \"" << args << "\" : expected \"AVG\" or integer for arg 3";
          this->camera.log_error( function, message.str() );
          return( ERROR );
        }
        catch ( std::out_of_range & ) {
          message.str(""); message << "parsing \"" << args << "\" : arg 3 outside integer range";
          this->camera.log_error( function, message.str() );
          return( ERROR );
        }

        // successfully converted value so check the range
        //
        if ( current_val < 0 || current_val > 1600000 ) {
          message.str(""); message << "requested current " << current_val << " outside range {0:1600000}";
          this->camera.log_error( function, message.str() );
          return( ERROR );
        }

        // prepare sensorconfig string for writing
        //
        readonly = false;
        sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "CURRENT";
        sensorvalue = tokens[2];
      }
    }
    else

    // If there are 4 tokens thenn it must be to write the average,
    // <module> < A | B | C > AVG N
    //
    if ( tokens.size() == 4 ) {        // set avg
      // check the contents of the 3rd arg
      //
      if ( tokens[2] != "AVG" ) {
        message.str(""); message << "invalid syntax \"" << tokens[2] << "\". expected <module> A|B|C AVG N";
      }

      // convert the avg value N to int and check for proper value
      int filter_val=-1;
      try {
        filter_val = std::stoi( tokens[3] );
      }
      catch ( std::invalid_argument & ) {
        message.str(""); message << "parsing \"" << args << "\" : expected integer for arg 4";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      catch ( std::out_of_range & ) {
        message.str(""); message << "parsing \"" << args << "\" : arg 4 outside integer range";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }

      // prepare sensorconfig string for writing
      //
      readonly = false;
      sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "FILTER";

      switch ( filter_val ) {
        case   1: sensorvalue = "0"; break;
        case   2: sensorvalue = "1"; break;
        case   4: sensorvalue = "2"; break;
        case   8: sensorvalue = "3"; break;
        case  16: sensorvalue = "4"; break;
        case  32: sensorvalue = "5"; break;
        case  64: sensorvalue = "6"; break;
        case 128: sensorvalue = "7"; break;
        case 256: sensorvalue = "8"; break;
          break;
        default:
          message.str(""); message << "requested average " << filter_val << " outside range {1,2,4,8,16,32,64,128,256}";
          this->camera.log_error( function, message.str() );
          return ( ERROR );
      }
    }

    // Otherwise an invalid number of tokens
    //
    else {
      message.str(""); message << "received " << tokens.size() << " arguments but expected 2, 3, or 4";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] module=" << module << " sensorid=" << sensorid
                             << " readonly=" << ( readonly ? "true" : "false" )
                             << " sensorconfig=" << sensorconfig.str()
                             << " sensorvalue=" << sensorvalue;
    logwrite( function, message.str() );
#endif

    std::string sensorkey = sensorconfig.str();

    if ( ! readonly ) {

      // should be impossible but just make sure these aren't empty because they're needed
      //
      if ( sensorconfig.rdbuf()->in_avail() == 0 || sensorvalue=="" ) {
        this->camera.log_error( function, "BUG DETECTED: sensorconfig and sensorvalue cannot be empty" );
        return ( ERROR );
      }

      // Write the config line to update the sensor current
      //
      bool changed = false;
      error = this->write_config_key( sensorkey.c_str(), sensorvalue.c_str(), changed );

      // Now send the APPLYMODx command
      //
      std::stringstream applystr;
      applystr << "APPLYMOD"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);

      if ( error == NO_ERROR ) error = this->archon_cmd( applystr.str() );

      message.str("");

      if ( error != NO_ERROR ) {
        message << "writing sensor configuration: " << sensorkey << "=" << sensorvalue;
      }
      else if ( !changed ) {
        message << "sensor configuration: " << sensorkey << "=" << sensorvalue << " unchanged";
      }
      else {
        message << "updated sensor configuration: " << sensorkey << "=" << sensorvalue;
      }
      logwrite( function, message.str() );
    }

    // now read back the configuration line
    //
    std::string value;
    error = this->get_configmap_value( sensorkey, value );

    if ( error != NO_ERROR ) {
      message.str(""); message << "reading sensor configuration " << sensorkey;
      logwrite( function, message.str() );
      return( error );  
    } 

    // return value to calling function, passed by reference
    //
    retstring = value;

    // if key ends with "FILTER" 
    // then convert the return value {0,1,2,...} to {1,2,4,8,...}
    //
    if ( sensorkey.substr( sensorkey.length()-6 ) == "FILTER" ) {

      // array of filter values that humans use
      //
      std::array< std::string, 9 > filter = { "1", "2", "4", "8", "16", "32", "64", "128", "256" };

      // the value in the configuration is an index into the above array
      //
      int findex=0;

      try {
        findex = std::stoi( value );
      }
      catch ( std::invalid_argument & ) {
        message.str(""); message << "bad value: " << value << " read back from configuration. expected integer";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      catch ( std::out_of_range & ) {
        message.str(""); message << "value: " << value << " read back from configuration outside integer range";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      try {
        retstring = filter.at( findex );         // return value to calling function, passed by reference
      }
      catch ( std::out_of_range & ) {
        message.str(""); message << "filter index " << findex << " outside range: {0:" << filter.size()-1 << "}";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }

    message.str(""); message << sensorkey << "=" << value << " (" << retstring << ")";
    logwrite( function, message.str() );

    return ( error );
  }
  /**************** Archon::Interface::sensor *********************************/


  /**************** Archon::Interface::bias ***********************************/
  /**
   * @fn     bias
   * @brief  set a bias
   * @param  args contains: module, channel, bias
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::bias(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::bias");
    std::stringstream message;
    std::vector<std::string> tokens;
    std::stringstream biasconfig;
    int module;
    int channel;
    float voltage;
    float vmin, vmax;
    bool readonly=true;

    // must have loaded firmware
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return( ERROR );
    }

    Tokenize(args, tokens, " ");

    if (tokens.size() == 2) {
      readonly = true;
    }
    else if (tokens.size() == 3) {
      readonly = false;
    }
    else {
      message.str(""); message << "incorrect number of arguments: " << args << ": expected module channel [voltage]";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    try {
      module  = std::stoi( tokens[0] );
      channel = std::stoi( tokens[1] );
      if (!readonly) voltage = std::stof( tokens[2] );
    }
    catch (std::invalid_argument &) {
      message.str(""); message << "parsing bias arguments: " << args << ": expected <module> <channel> [ voltage ]";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }
    catch (std::out_of_range &) {
      message.str(""); message << "argument range: " << args << ": expected <module> <channel> [ voltage ]";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // Check that the module number is valid
    //
    if ( (module < 0) || (module > nmods) ) {
      message.str(""); message << "module " << module << ": outside range {0:" << nmods << "}";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // Use the module type to get LV or HV Bias
    // and start building the bias configuration string.
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return(ERROR);
        break;
      case 3:  // LVBias
      case 9:  // LVXBias
        biasconfig << "MOD" << module << "/LV";
        vmin = -14.0;
        vmax = +14.0;
        break;
      case 4:  // HVBias
      case 8:  // HVXBias
        biasconfig << "MOD" << module << "/HV";
        vmin =   0.0;
        vmax = +31.0;
        break;
      default:
        message.str(""); message << "module " << module << " not a bias board";
        this->camera.log_error( function, message.str() );
        return(ERROR);
        break;
    }

    // Check that the channel number is valid
    // and add it to the bias configuration string.
    //
    if ( (channel < 1) || (channel > 30) ) {
      message.str(""); message << "bias channel " << module << ": outside range {1:30}";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }
    if ( (channel > 0) && (channel < 25) ) {
      biasconfig << "LC_V" << channel;
    }
    if ( (channel > 24) && (channel < 31) ) {
      channel -= 24;
      biasconfig << "HC_V" << channel;
    }

    if ( (voltage < vmin) || (voltage > vmax) ) {
      message.str(""); message << "bias voltage " << voltage << ": outside range {" << vmin << ":" << vmax << "}";
      this->camera.log_error( function, message.str() );
      return(ERROR);
    }

    // Locate this line in the configuration so that it can be written to the Archon
    //
    std::string key   = biasconfig.str();
    std::string value = std::to_string(voltage);
    bool changed      = false;
    long error;

    // If no voltage suppled (readonly) then just read the configuration and exit
    //
    if (readonly) {
      message.str("");
      error = this->get_configmap_value(key, voltage);
      if (error != NO_ERROR) {
        message << "reading bias " << key;
      }
      else {
        retstring = std::to_string(voltage);
        message << "read bias " << key << "=" << voltage;
      }
      logwrite( function, message.str() );
      return error;
    }

    // Write the config line to update the bias voltage
    //
    error = this->write_config_key(key.c_str(), value.c_str(), changed);

    // Now send the APPLYMODx command
    //
    std::stringstream applystr;
    applystr << "APPLYMOD" 
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << (module-1);

    if (error == NO_ERROR) error = this->archon_cmd(applystr.str());

    if (error != NO_ERROR) {
      message << "writing bias configuration: " << key << "=" << value;
    }
    else if (!changed) {
      message << "bias configuration: " << key << "=" << value <<" unchanged";
    }
    else {
      message << "updated bias configuration: " << key << "=" << value;
    }

    logwrite(function, message.str());

    return error;
  }
  /**************** Archon::Interface::bias ***********************************/


  /**************** Archon::Interface::cds ************************************/
  /**
   * @fn     cds
   * @brief  set / get CDS parameters
   * @param  
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::cds(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::cds");
    std::stringstream message;
    std::vector<std::string> tokens;
    std::string key, value;
    bool changed;
    long error = ERROR;

    if ( args.empty() ) {
      this->camera.log_error( function, "no argument: expected cds <configkey> [ value ]" );
      return( ERROR );
    }

    try {
      Tokenize(args, tokens, " ");

      // One token --
      // get the configuration key value
      //
      if ( tokens.size() == 1 ) {
        key   = tokens.at(0);
        std::transform( key.begin(), key.end(), key.begin(), ::toupper );          // make uppercase
        error = this->get_configmap_value(key, retstring);                         // read
      }
      else

      // Two tokens --
      // set the configuration key to value, send APPLYCDS, then read back the config key
      //
      if ( tokens.size() == 2 ) {
        key   = tokens.at(0);
        std::transform( key.begin(), key.end(), key.begin(), ::toupper );          // make uppercase
        value = tokens.at(1);
        error = this->write_config_key( key.c_str(), value.c_str(), changed );     // set
        if (error == NO_ERROR) error = this->archon_cmd(APPLYCDS);                 // apply
        if (error == NO_ERROR) error = this->get_configmap_value(key, retstring);  // read back
      }

      // More than two tokens is an error
      //
      else {
        this->camera.log_error( function, "Too many arguments. Expected cds <configkey> [ value ]" );
        return( ERROR );
      }
    }
    catch(std::out_of_range &) {
      message << "parsing cds arguments: " << args << ": Expected cds <configkey> [ value ]";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }
    catch(...) {
      message << "unknown exception parsing cds arguments: " << args << ": Expected cds <configkey> [ value ]";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    return( error );
  }
  /**************** Archon::Interface::cds ************************************/


  /**************** Archon::Interface::inreg **********************************/
  /**
   * @fn     inreg
   * @brief  write to a VCPU INREGi
   * @param  args contains: module inreg value
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::inreg( std::string args ) {
    const std::string function("Archon::Interface::inreg");
    std::stringstream message;
    std::vector<std::string> tokens;
    int module, reg, value;

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return( ERROR );
    }

    // VCPU requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_VCPU );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_VCPU;
      }
      else {
        message << "requires backplane version " << REV_VCPU << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    Tokenize(args, tokens, " ");

    // There must be three tokens, <module> <reg> <value>
    //
    if ( tokens.size() != 3 ) {
      this->camera.log_error( function, "expected three arguments: <module> <reg> <value>" );
      return ERROR;
    }

    // Now get the module and 
    // which will be common to everything.
    //
    try {
      module = std::stoi( tokens[0] );
      reg    = std::stoi( tokens[1] );
      value  = std::stoi( tokens[2] );
    }
    catch (std::invalid_argument &) {
      message.str(""); message << "unable to convert one of \"" << args << "\" to integer";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }
    catch (std::out_of_range &) {
      message.str(""); message << "one of \"" << args << "\" is outside integer range";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    // check that requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "requested module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      case 3:  // LVBias
      case 5:  // Heater
      case 7:  // HS
      case 9:  // LVXBias
      case 10: // LVDS
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "requested module " << module << " does not contain a VCPU";
        this->camera.log_error( function, message.str() );
        return( ERROR );
    }

    // check that register number is valid
    //
    if ( reg < 0 || reg > 15 ) {
      message.str(""); message << "requested register " << reg << " outside range {0:15}";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    // check that value is within range
    //
    if ( value < 0 || value > 65535 ) {
      message.str(""); message << "requested value " << value << " outside range {0:65535}";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    std::stringstream inreg_key;
    bool changed = false;
    inreg_key << "MOD" << module << "/VCPU_INREG" << reg;
    long error = this->write_config_key( inreg_key.str().c_str(), value, changed );
    if ( error != NO_ERROR ) {
      message.str(""); message << "configuration " << inreg_key.str() << "=" << value;
      logwrite( function, message.str() );
      return( ERROR );
    }
    else {
      std::stringstream applystr;
      applystr << "APPLYDIO"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);
      error = this->archon_cmd( applystr.str() );
      return( error );
    }
  }
  /**************** Archon::Interface::inreg **********************************/


  /***** Archon::Interface::readout *******************************************/
  /**
   * @brief  
   * @param[in]  readout_in   string containing the requested readout type
   * @param[out] readout_out  reference to a string for return values
   * @return     ERROR or NO_ERROR
   *
   * This function sets (or gets) the type of readout. This will encompass
   * which amplifier(s) for a CCD (by name) or infrared (by number).
   * Selecting the readout will also set the appropriate deinterlacing
   * scheme to be used.
   *
   */
  long Interface::readout( std::string readout_in, std::string &readout_out ) {
    const std::string function("Archon::Interface::readout");
    std::stringstream message;
    std::vector<std::string> tokens;
    long error = NO_ERROR;

    uint32_t _readout_arg;                  // argument associated with requested type
    int _readout_type;
    bool readout_name_valid = false;

    try {
      std::transform( readout_in.begin(), readout_in.end(), readout_in.begin(), ::toupper );    // make uppercase
    }
    catch (...) {
      message.str(""); message << "converting readout type: " << readout_in << " to uppercase";
      this->camera.log_error( function, message.str() );
      return( ERROR );
    }

    // Special case -- if requested a list then return a list of accepted readout amp names
    //
    if ( readout_in == "LIST" ) {
      std::stringstream rs;
      for ( auto type : this->readout_source ) rs << type.first << " ";
      readout_out = rs.str();
      logwrite( function, rs.str() );
      return( NO_ERROR );
    }
    else

    // Otherwise, the argument is a requested readout type
    //
    if ( not readout_in.empty() ) {

      // Check that the requested readout amplifer has a matches in the list of known readout amps.
      //
      for ( auto source : this->readout_source ) {
        if ( source.first.compare( readout_in ) == 0 ) {  // found a match
          readout_name_valid = true;
          _readout_arg  = source.second.readout_arg;      // get the arg associated with this match
          _readout_type = source.second.readout_type;     // get the type associated with this match
          break;
        }
      }
      if ( !readout_name_valid ) {
        message.str(""); message << "ERROR: readout " << readout_in << " not recognized";
        logwrite( function, message.str() );
        error = ERROR;
      }
      else {  // requested readout type is known, so set it for each of the specified devices
        this->camera_info.readout_name = readout_in;
        this->camera_info.readout_type = _readout_type;
        this->readout_arg = _readout_arg;
      }
    }

    readout_out = this->camera_info.readout_name;

    message.str(""); message << "readout type " << readout_out;
    logwrite( function, message.str() );

    return( error );
  }
  /***** Archon::Interface::readout *******************************************/


  /***** Archon::Interface::caltimer ******************************************/
  /**
   * @brief      calibrate the Archon TIMER with the system time
   * @return     NO_ERROR if successful or ERROR on error
   *
   * This records the Archon TIMER value together with the host's system time
   * so that future readings of the Archon timer can be correlated with a real
   * clock time. This value should be accurate to about 10µsec. The stability
   * of this "calibration" isn't well known so consider doing this at least once
   * per day, if not more frequently.
   *
   * These values will be added to the systemkeys database. The Archon TIMER is
   * stored as a string (keyword CAL_ARCH) so that its 64 bit value doesn't
   * overload the CCfits container. The system time (CAL_SYS) is stored as 
   * YYYY-MM-DDTHH:MM:SS.ssssss
   *
   */
  long Interface::caltimer() {
    const std::string function("Archon::Interface::caltimer");
    std::stringstream message;
    long error = NO_ERROR;

    // Archon's CPU is single-threaded. Temporarily disabling hardware polling will
    // ensure a prompt response to the TIMER command.
    //
    error = this->archon_cmd(POLLOFF);

    if ( error == NO_ERROR ) error = get_timer( &this->cal_archontime );  // get Archon TIMER, store in unsigned long
    if ( error == NO_ERROR ) {
      error = clock_gettime( CLOCK_REALTIME, &this->cal_systime );        // get system time, store in timespec struct
      if ( error != 0 ) {
        logwrite( function, "ERROR getting system time" );
        error = ERROR;
      }
    }

    // Re-enable hardware polling
    //
    error |= this->archon_cmd(POLLON);  // OR'd so as not to lose any previous error

    message.str(""); message << "Archon time at " << timestamp_from( this->cal_systime ) << " is " << this->cal_archontime;
    logwrite( function, message.str() );

    // Add the calibration values to the systemkeys database
    //
    message.str(""); message << "CAL_ARCH=" << this->cal_archontime << "// Archon time in 10ns per tick at CAL_SYS";
    this->systemkeys.addkey( message.str() );
    message.str(""); message << "CAL_ARCH:" << this->cal_archontime;
    this->camera.async.enqueue( message.str() );

    message.str(""); message << "CAL_SYS=" << timestamp_from( this->cal_systime ) << "// system time at CAL_ARCH";
    this->systemkeys.addkey( message.str() );
    message.str(""); message << "CAL_SYS:" << timestamp_from( this->cal_systime );
    this->camera.async.enqueue( message.str() );

    return( error );
  }
  /***** Archon::Interface::caltimer ******************************************/


  /**************** Archon::Interface::test ***********************************/
  /**
   * @fn     test
   * @brief  test routines
   * @param  string args contains test name and arguments
   * @param  reference to retstring for any return values
   * @return ERROR or NO_ERROR
   *
   * This is the place to put various debugging and system testing tools.
   * It is placed here, rather than in camera, to allow for controller-
   * specific tests. This means some common tests may need to be duplicated
   * for each controller.
   *
   * These are kept out of server.cpp so that I don't need a separate,
   * potentially conflicting command for each test I come up with, and also
   * reduces the chance of someone accidentally, inadvertently entering one
   * of these test commands.
   *
   * The server command is "test", the next parameter is the test name,
   * and any parameters needed for the particular test are extracted as
   * tokens from the args string passed in.
   *
   * The input args string is tokenized and tests are separated by a simple
   * series of if..else.. conditionals.
   *
   * current tests are:
   *   ampinfo   - print what is known about the amplifiers
   *   busy      - Override the archon_busy flag
   *   fitsname  - show what the fitsname will look like
   *   builddate - log the build date
   *   async     - queue an asynchronous status message
   *   modules   - Log all installed modules and their types
   *   parammap  - Log all parammap entries found in the ACF file
   *   configmap - Log all configmap entries found in the ACF file
   *   bw        - tests the exposure sequence bandwidth by running a sequence of exposures
   *   timer     - test Archon time against system time
   */
  long Interface::test(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::test");
    std::stringstream message;
    std::vector<std::string> tokens;
    long error;

    Tokenize(args, tokens, " ");

    if (tokens.size() < 1) {
      this->camera.log_error( function, "no test name provided" );
      return ERROR;
    }

    std::string testname;
    try { testname = tokens.at(0); }                                 // the first token is the test name
    catch ( std::out_of_range & ) { this->camera.log_error( function, "testname token out of range" ); return( ERROR ); }

    // ----------------------------------------------------
    // ampinfo
    // ----------------------------------------------------
    // print what is known about the amplifiers
    //
    if (testname == "ampinfo") {

      std::string mode = this->camera_info.current_observing_mode;
      int framemode = this->modemap[mode].geometry.framemode;

      message.str(""); message << "[ampinfo] observing mode=" << mode;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] FRAMEMODE=" << this->modemap[mode].geometry.framemode;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] LINECOUNT=" << this->modemap[mode].geometry.linecount << " PIXELCOUNT=" << this->modemap[mode].geometry.pixelcount;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] num_taps=" << this->modemap[mode].tapinfo.num_taps;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] hori_amps=" << this->modemap[mode].geometry.amps[0] << " vert_amps=" << this->modemap[mode].geometry.amps[1];
      logwrite( function, message.str() );

      message.str(""); message << "[ampinfo] gains =";
      for ( auto _gain : this->gain ) {
        message << " " << _gain;
      }
      logwrite( function, message.str() );

      int rows = this->modemap[mode].geometry.linecount;
      int cols = this->modemap[mode].geometry.pixelcount;

      int hamps = this->modemap[mode].geometry.amps[0];
      int vamps = this->modemap[mode].geometry.amps[1];

      int x0=-1, x1, y0, y1;

      for ( int y=0; y<vamps; y++ ) {
        for ( int x=0; x<hamps; x++ ) {
          if ( framemode == 2 ) {
            x0 = x; x1=x+1;
            y0 = y; y1=y+1;
          }
          else {
            x0++;   x1=x0+1;
            y0 = 0; y1=1;
          }
          message.str(""); message << "[ampinfo] x0=" << x0 << " x1=" << x1 << " y0=" << y0 << " y1=" << y1 
                                   << " | amp section (xrange, yrange) " << (x0*cols + 1) << ":" << (x1)*cols << ", " << (y0*rows + 1) << ":" << (y1)*rows;
          logwrite( function, message.str() );
        }
      }
      error = NO_ERROR;
    }

    // ----------------------------------------------------
    // busy
    // ----------------------------------------------------
    // Override the archon_busy flag to test system responsiveness
    // when the Archon is busy.
    //
    else
    if (testname == "busy") {
      if ( tokens.size() == 1 ) {  // fall through
      }
      else if ( tokens.size() == 2 ) {
        if ( tokens[1] == "set" ) {
          this->archon_busy.test_and_set();
        }
        else
        if ( tokens[1] == "clear" ) {
          this->archon_busy.clear();
        }
        else {
          logwrite( function, "ERROR expected set | clear" );
          error = NO_ERROR;
        }
      }
      else {
        this->camera.log_error( function, "ERROR expected set | clear" );
        error = ERROR;
      }

      if ( this->archon_busy.test_and_set() ) {  // was already set
        retstring="set";
      }
      else {                                     // was clear, so clear before returning
        this->archon_busy.clear();
        retstring="clear";
      }
      logwrite( function, retstring );
      error = NO_ERROR;
    }

    // ----------------------------------------------------
    // fitsname
    // ----------------------------------------------------
    // Show what the fitsname will look like.
    // This is a "test" rather than a regular command so that it doesn't get mistaken
    // for returning a real, usable filename. When using fitsnaming=time, the filename
    // has to be generated at the moment the file is opened.
    //
    else
    if (testname == "fitsname") {
      std::string msg;
      this->camera.set_fitstime( get_timestamp() );                  // must set camera.fitstime first
      error = this->camera.get_fitsname(msg);                        // get the fitsname (by reference)
      retstring = msg;
      message.str(""); message << "NOTICE:" << msg;
      this->camera.async.enqueue( message.str() );                   // queue the fitsname
      logwrite(function, msg);                                       // log the fitsname
      if (error!=NO_ERROR) { this->camera.log_error( function, "couldn't validate fits filename" ); }
    } // end if (testname == fitsname)

    // ----------------------------------------------------
    // builddate
    // ----------------------------------------------------
    // log the build date
    //
    else
    if ( testname == "builddate" ) {
      std::string build(BUILD_DATE); build.append(" "); build.append(BUILD_TIME);
      retstring = build;
      error = NO_ERROR;
      logwrite( function, build );
    } // end if ( testname == builddate )

    // ----------------------------------------------------
    // async [message]
    // ----------------------------------------------------
    // queue an asynchronous message
    // The [message] param is optional. If not provided then "test" is queued.
    //
    else
    if (testname == "async") {
      if (tokens.size() > 1) {
        if (tokens.size() > 2) {
          logwrite(function, "NOTICE:received multiple strings -- only the first will be queued");
        }
        try { message.str(""); message << "NOTICE:" << tokens.at(1);
              logwrite( function, message.str() );
              this->camera.async.enqueue( message.str() );
        }
        catch ( std::out_of_range & ) { this->camera.log_error( function, "tokens out of range" ); error=ERROR; }
      }
      else {                                // if no string passed then queue a simple test message
        logwrite(function, "NOTICE:test");
        this->camera.async.enqueue("NOTICE:test");
      }
      error = NO_ERROR;
    } // end if (testname == async)

    // ----------------------------------------------------
    // modules
    // ----------------------------------------------------
    // Log all installed modules
    //
    else
    if (testname == "modules") {
      logwrite( function, "installed module types: " );
      message.str("");
      for ( auto mod : this->modtype ) {
        message << mod << " ";
      }
      logwrite( function, message.str() );
      retstring = message.str();
      error = NO_ERROR;
    }

    // ----------------------------------------------------
    // parammap
    // ----------------------------------------------------
    // Log all parammap entries found in the ACF file
    //
    else
    if (testname == "parammap") {

      // loop through the modes
      //
      logwrite(function, "parammap entries by mode section:");
      for (auto mode_it = this->modemap.begin(); mode_it != this->modemap.end(); ++mode_it) {
        std::string mode = mode_it->first;
        message.str(""); message << "found mode section " << mode;
        logwrite(function, message.str());
        for (auto param_it = this->modemap[mode].parammap.begin(); param_it != this->modemap[mode].parammap.end(); ++param_it) {
          message.str(""); message << "MODE_" << mode << ": " << param_it->first << "=" << param_it->second.value;
          logwrite(function, message.str());
        }
      }

      logwrite(function, "ALL parammap entries in ACF:");
      int keycount=0;
      for (auto param_it = this->parammap.begin(); param_it != this->parammap.end(); ++param_it) {
        keycount++;
        message.str(""); message << param_it->first << "=" << param_it->second.value;
        logwrite(function, message.str());
        this->camera.async.enqueue( "NOTICE:"+message.str() );
      }
      message.str(""); message << "found " << keycount << " parammap entries";
      logwrite(function, message.str());
      error = NO_ERROR;
    } // end if (testname == parammap)

    // ----------------------------------------------------
    // configmap
    // ----------------------------------------------------
    // Log all configmap entries found in the ACF file
    //
    else
    if (testname == "configmap") {
      error = NO_ERROR;
      logwrite(function, "configmap entries by mode section:");
      for (auto mode_it=this->modemap.begin(); mode_it!=this->modemap.end(); ++mode_it) {
        std::string mode = mode_it->first;
        message.str(""); message << "found mode section " << mode;
        logwrite(function, message.str());
        for (auto config_it = this->modemap[mode].configmap.begin(); config_it != this->modemap[mode].configmap.end(); ++config_it) {
          message.str(""); message << "MODE_" << mode << ": " << config_it->first << "=" << config_it->second.value;
          logwrite(function, message.str());
        }
      }

      // if a second argument was passed then this is a config key
      // try to read it
      //
      if ( tokens.size() == 2 ) {
        try {
          std::string configkey = tokens.at(1);
          error = this->get_configmap_value(configkey, retstring);
        }
        catch ( std::out_of_range & ) { this->camera.log_error( function, "configkey token out of range" ); error=ERROR; }
      }

      // if a third argument was passed then set this configkey
      //
      if ( tokens.size() == 3 ) {
        try { std::string key = tokens.at(1);
              std::string value = tokens.at(2);
              bool configchanged;
              error = this->write_config_key( key.c_str(), value.c_str(), configchanged );
              if (error == NO_ERROR) error = this->archon_cmd(APPLYCDS);
        }
        catch ( std::out_of_range & ) { this->camera.log_error( function, "key,value tokens out of range" ); error=ERROR; }
      }

      int keycount=0;
      for (auto config_it = this->configmap.begin(); config_it != this->configmap.end(); ++config_it) {
        keycount++;
      }
      message.str(""); message << "found " << keycount << " configmap entries";
      logwrite(function, message.str());
    } // end if (testname == configmap)

    // ----------------------------------------------------
    // bw <nseq>
    // ----------------------------------------------------
    // Bandwidth test
    // This tests the exposure sequence bandwidth by running a sequence
    // of exposures, including reading the frame buffer -- everything except
    // for the fits file writing.
    //
    else
    if (testname == "bw") {

      if ( ! this->modeselected ) {
        this->camera.log_error( function, "no mode selected" );
        return ERROR;
      }

      std::string nseqstr;
      int nseq;
      bool ro=false;  // read only
      bool rw=false;  // read and write

      if (tokens.size() > 1) {
        try { nseqstr = tokens.at(1); }
        catch ( std::out_of_range & ) { this->camera.log_error( function, "nseqstr token out of range" ); return(ERROR); }
      }
      else {
        this->camera.log_error( function, "usage: test bw <nseq> [ rw | ro ]");
        return ERROR;
      }

      if (tokens.size() > 2) {
        try { if (tokens.at(2) == "rw") rw=true; else rw=false;
              if (tokens.at(2) == "ro") ro=true; else ro=false;
        }
        catch ( std::out_of_range & ) { this->camera.log_error( function, "rw tokens out of range" ); error=ERROR; }
      }

      try {
        nseq = std::stoi( nseqstr );                                // test that nseqstr is an integer before trying to use it
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "unable to convert sequences: " << nseqstr << " to integer";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "sequences " << nseqstr << " outside integer range";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }

      // abortparam is set by the configuration file
      // check to make sure it was set, or else expose won't work
      //
      if (this->abortparam.empty()) {
        message.str(""); message << "ABORT_PARAM not defined in configuration file " << this->config.filename;
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }

      // exposeparam is set by the configuration file
      // check to make sure it was set, or else expose won't work
      //
      if (this->exposeparam.empty()) {
        message.str(""); message << "EXPOSE_PARAM not defined in configuration file " << this->config.filename;
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
      error = this->get_frame_status();  // TODO is this needed here?

      if (error != NO_ERROR) {
        logwrite( function, "ERROR: unable to get frame status" );
        return(ERROR);
      }
      this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

      // initiate the exposure here
      //
      error = this->prep_parameter(this->exposeparam, nseqstr);
      if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);

      // get system time and Archon's timer after exposure starts
      // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
      //
      if (error == NO_ERROR) {
        this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
        error = this->get_timer(&this->start_timer);                  // Archon internal timer (one tick=10 nsec)
        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: couldn't get start time" );
          return( error );
        }
        this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
        // If read-write selected then need to do some FITS stuff
        //
        if ( rw ) {
          this->camera_info.extension.store(0);                       // always initialize extension
          error = this->camera.get_fitsname( this->camera_info.fits_name ); // assemble the FITS filename if rw selected
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: couldn't validate fits filename" );
            return( error );
          }
          this->add_filename_key();                                   // add filename to system keys database
          Common::FitsKeys::fits_key_t::iterator keyit;               // add keys from the ACF file 
          for (keyit  = this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.begin();
               keyit != this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.end();
               keyit++) {
            this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
          }

          this->camera_info.ismex = this->camera.mex();

          // open the file now for multi-extensions
          //
          if ( this->camera.mex() ) {
            this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
//          error = this->xfits_file.open_file( (this->camera.writekeys_when=="before"?true:false), this->camera_info );
            if ( error != NO_ERROR ) {
              this->camera.log_error( function, "couldn't open fits file" );
              return( error );
            }
          }
        }
      }

      if (error == NO_ERROR) logwrite(function, "exposure started");

      long frames_read = 0;

      // Wait for Archon frame buffer to be ready, then read the latest ready frame buffer to the host.
      // Loop over all expected frames.
      //
      while (nseq-- > 0) {

        // If read-write selected,
        // Open a new FITS file for each frame when not using multi-extensions
        //
        if ( rw && !this->camera.mex() ) {
          this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
          if ( this->get_timer(&this->start_timer) != NO_ERROR ) {      // Archon internal timer (one tick=10 nsec)
            logwrite( function, "ERROR: couldn't get start time" );
            return( error );
          }
          this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
          error=this->camera.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: couldn't validate fits filename" );
            return( error );
          }
          this->add_filename_key();                                     // add filename to system keys database

          this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
//        error = this->xfits_file.open_file( (this->camera.writekeys_when=="before"?true:false), this->camera_info );
          if ( error != NO_ERROR ) {
            this->camera.log_error( function, "couldn't open fits file" );
            return( error );
          }
        }

        if (this->camera_info.exposure_delay != 0) {                 // wait for the exposure delay to complete (if there is one)
          error = this->wait_for_exposure();
          if (error==ERROR) {
            logwrite( function, "ERROR: exposure delay error" );
            break;
          }
          else {
            logwrite(function, "exposure delay complete");
          }
        }

        if (error==NO_ERROR) error = this->wait_for_readout();                     // wait for the readout into frame buffer,
        if (error==NO_ERROR && ro) error = this->read_frame(Camera::FRAME_IMAGE);  // read image frame directly with no write
        if (error==NO_ERROR && rw) error = this->read_frame();                     // read (and write) image frame directly
        if (error==NO_ERROR && rw && !this->camera.mex()) {
//        this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info );
          this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
          this->camera.increment_imnum();                                          // increment image_num when fitsnaming == "number"
        }
        if (error==NO_ERROR) frames_read++;
      }
      retstring = std::to_string( frames_read );

      // for multi-extensions, close the FITS file now that they've all been written
      // (or any time there is an error)
      //
      if ( rw && ( this->camera.mex() || (error==ERROR) ) ) {
//      this->xfits_file.close_file( (this->camera.writekeys_when=="after"?true:false), this->camera_info );
        this->camera_info.writekeys_before = (this->camera.writekeys_when=="before"?true:false);
        this->camera.increment_imnum();                                            // increment image_num when fitsnaming == "number"
      }

      logwrite( function, (error==ERROR ? "ERROR" : "complete") );

      message.str(""); message << "frames read = " << frames_read;
      logwrite(function, message.str());

    } // end if (testname==bw)

    // ----------------------------------------------------
    // timer
    // ----------------------------------------------------
    // test Archon time against system time
    //
    else
    if (testname == "timer") {

      int nseq;
      int sleepus;
      double systime1, systime2;
      uint64_t archontime1, archontime2;
      std::vector<long long> deltatime;
      long long delta_archon, delta_system;

      if (tokens.size() < 3) {
        this->camera.log_error( function, "expected test timer <cycles> <sleepus>" );
        return ERROR;
      }

      try {
        nseq    = std::stoi( tokens.at(1) );
        sleepus = std::stoi( tokens.at(2) );
      }
      catch (std::invalid_argument &) {
        message.str(""); message << "unable to convert one or more args to an integer";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        message.str(""); message << "nseq, sleepus tokens outside range";
        this->camera.log_error( function, message.str() );
        return(ERROR);
      }

      error = NO_ERROR;

      // turn off background polling while doing the timing test
      //
      if (error == NO_ERROR) error = this->archon_cmd(POLLOFF);

      // send the Archon TIMER command here, nseq times, with sleepus delay between each
      //
      int nseqsave = nseq;
      while ( error==NO_ERROR && nseq-- > 0 ) {
        // get Archon timer [ns] and system time [s], twice
        //
        error = get_timer(&archontime1);
        systime1 = get_clock_time();
        error = get_timer(&archontime2);
        systime2 = get_clock_time();

        // difference between two calls, converted to µsec
        //
        if ( archontime2 > archontime1 ) {
          delta_archon = static_cast<long long>(archontime2) - static_cast<long long>(archontime1);
          delta_archon /= 100;                                                      // archon time was in 10 nsec
        }
        else {
          message.str(""); message << "ERROR archontime2 " << archontime2
                                   << " not greater than archontime1 " << archontime1;
          logwrite( function, message.str() );
          return ERROR;
        }

        delta_system = static_cast<long long>((systime2 - systime1) * 1000000.);  // system time was in sec

        // enque each line to the async message port
        //
        message.str(""); message << "TEST_TIMER: " << nseqsave-nseq << ", " << delta_archon << ", " << delta_system;
        this->camera.async.enqueue( message.str() );

        // save the difference between 
        //
        deltatime.push_back( abs( delta_archon - delta_system ) );

        std::this_thread::sleep_for( std::chrono::microseconds( sleepus ) );
      }

      // background polling back on
      //
      if (error == NO_ERROR) error = this->archon_cmd(POLLON);

      // calculate the average and standard deviation of the difference
      // between system and archon
      //
      uint32_t _deltatimesize = static_cast<uint32_t>(deltatime.size());
      if ( _deltatimesize < 1 ) {
        logwrite( function, "ERROR no time" );
        return ERROR;
      }
      long long sum = std::accumulate(deltatime.begin(), deltatime.end(), 0LL);
      double m =  static_cast<double>(sum) / _deltatimesize;

      double accum = 0.0;
      std::for_each (deltatime.begin(), deltatime.end(), [&](const long long d) {
          accum += (static_cast<double>(d) - m) * (static_cast<double>(d) - m);
      });

      double stdev = 0.;
      if ( _deltatimesize > 1 ) stdev = sqrt(accum / static_cast<double>(_deltatimesize-1));

      message.str(""); message << "average delta=" << m << " stddev=" << stdev;
      logwrite(function, message.str());

      retstring = "delta=" + std::to_string( m ) + " stddev=" + std::to_string( stdev );

    } // end if (testname==timer)

    // ----------------------------------------------------
    // invalid test name
    // ----------------------------------------------------
    //
    else {
      message.str(""); message << "unknown test: " << testname;
      this->camera.log_error( function, message.str() );
      error = ERROR;
    }

    return error;
  }
  /**************** Archon::Interface::test ***********************************/


  /***** Archon::Interface::abort *********************************************/
  /**
   * @brief      set abort flags, locally and in Archon
   * @return     ERROR | NO_ERROR
   *
   */
  long Interface::abort() {
    const std::string function("Archon::Interface::abort");
    std::stringstream message;

    // Tell Archon to abort (the ACF must support this)
    // If Archon is reading out then this will block until readout is complete.
    // There is no way to stop a FETCH command once started.
    //
    long error = this->abort_archon();

    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR aborting Archon" );
    }

    // Set class object abort flags
    //
    this->camera.set_abort();
    this->camera_info.exposure_aborted = true;
    if ( this->camera_info.iscds ) this->cds_info.exposure_aborted = true;

    return error;
  }
  /***** Archon::Interface::abort *********************************************/


  /***** Archon::Interface::abort_archon **************************************/
  /**
   * @brief      set the abort parameter on the Archon
   * @return     ERROR | NO_ERROR
   *
   */
  long Interface::abort_archon() {
    logwrite( "Archon::Interface::abort_archon", "setting Archon abort parameter" );
    long error = this->prep_parameter( this->abortparam, "1" );
    if ( error == NO_ERROR ) error = this->load_parameter( this->abortparam, "1" );
    return error;
  }
  /***** Archon::Interface::abort_archon **************************************/


  /***** Archon::Interface::alloc_workbuf *************************************/
  /**
   * @brief      allocate workspace memory for deinterlacing
   * @return     ERROR or NO_ERROR
   *
   * @todo       I think this is obsolete now that I switched to a ring buffer 2/14/23
   *
   * This function calls an overloaded template class version with 
   * a generic pointer cast to the correct type.
   *
   */
  long Interface::alloc_workbuf() {
    const std::string function("Archon::Interface::alloc_workbuf");
    std::stringstream message;
    long retval = NO_ERROR;
    void* ptr=nullptr;

    switch ( this->camera_info.datatype ) {
      case USHORT_IMG: {
        this->alloc_workring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->alloc_workring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->alloc_workring( (uint32_t *)ptr );
        break;
      }
      default:
        message.str(""); message << "unknown datatype: " << this->camera_info.datatype;
        this->camera.log_error(function, message.str());
        retval = ERROR;
        break;
    }

    return( retval );
  }
  /***** Archon::Interface::alloc_workbuf *************************************/


  /***** Archon::Interface::alloc_workbuf *************************************/
  /**
   * @brief      allocate workspace memory for deinterlacing
   * @param[in]  buf, pointer to template type T
   * @return     pointer to the allocated space
   *
   * @todo       I think this is obsolete now that I switched to a ring buffer 2/14/23
   *
   * The actual allocation occurs in here, based on the template class pointer type.
   *
   */
  template <class T>
  void* Interface::alloc_workbuf(T* buf) {
    const std::string function("Archon::Interface::alloc_workbuf");
    std::stringstream message;

    // Maybe the size of the existing buffer is already just right
    //
    if ( this->camera_info.section_size == this->workbuf_size ) return( (void*)this->workbuf );

    // But if it's not, then free whatever space is allocated, ...
    //
    if ( this->workbuf != nullptr ) this->free_workbuf(buf);

    // ...and then allocate new space.
    //
    this->workbuf = (T*) new T [ this->camera_info.section_size ]{};
    this->workbuf_size = this->camera_info.section_size;

    message << "allocated " << this->workbuf_size << " pixels for deinterlacing buffer " << std::hex << (void*)this->workbuf;
    logwrite(function, message.str());
    return( (void*)this->workbuf );
  }
  /***** Archon::Interface::alloc_workbuf *************************************/


  /***** Archon::Interface::alloc_workring ************************************/
  /**
   * @brief      allocate workspace memory for deinterlacing
   * @return     ERROR or NO_ERROR
   *
   * This function calls an overloaded template class version with 
   * a generic pointer cast to the correct type.
   *
   */
  long Interface::alloc_workring() {
    const std::string function("Archon::Interface::alloc_workring");
    std::stringstream message;
    long retval = NO_ERROR;
    void* ptr=nullptr;

    switch ( this->camera_info.datatype ) {
      case USHORT_IMG: {
        this->alloc_workring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->alloc_workring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->alloc_workring( (uint32_t *)ptr );
        break;
      }
      case LONG_IMG: {
        this->alloc_workring( (int32_t *)ptr );
        break;
      }
      default:
        message.str(""); message << "cannot allocate for unknown datatype: " << this->camera_info.datatype;
        this->camera.log_error(function, message.str());
        retval = ERROR;
        break;
    }

    return( retval );
  }
  /***** Archon::Interface::alloc_workring ************************************/


  /***** Archon::Interface::alloc_cdsring *************************************/
  /**
   * @brief      allocate workspace memory for cds ring buffer
   * @return     ERROR or NO_ERROR
   *
   * This function calls an overloaded template class version with 
   * a generic pointer cast to the correct type.
   *
   */
  long Interface::alloc_cdsring() {
    const std::string function("Archon::Interface::alloc_cdsring");
    std::stringstream message;
    long retval = NO_ERROR;
    void* ptr=nullptr;

//  if ( not this->camera_info.iscds ) return( NO_ERROR );

    switch ( this->cds_info.datatype ) {
      case USHORT_IMG: {
        this->alloc_cdsring( (uint16_t *)ptr );
        break;
      }
      case SHORT_IMG: {
        this->alloc_cdsring( (int16_t *)ptr );
        break;
      }
      case FLOAT_IMG: {
        this->alloc_cdsring( (uint32_t *)ptr );
        break;
      }
      case LONG_IMG: {
        this->alloc_cdsring( (int32_t *)ptr );
        break;
      }
      default:
        message.str(""); message << "cannot allocate for unknown datatype: " << this->cds_info.datatype;
        this->camera.log_error(function, message.str());
        retval = ERROR;
        break;
    }

    return( retval );
  }
  /***** Archon::Interface::alloc_cdsring *************************************/


  /***** Archon::Interface::alloc_cdsring *************************************/
  /**
   * @brief      allocate workspace memory for a cds ring buffer
   * @param[in]  buf, pointer to template type T
   * @return     pointer to the allocated space
   *
   * The actual allocation occurs in here, based on the template class pointer type.
   *
   */
  template <class T>
  void Interface::alloc_cdsring( T* buf ) {
    const std::string function("Archon::Interface::alloc_cdsring");
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] cds_info.section_size=" << this->cds_info.section_size << " cdsbuf_size=" << this->cdsbuf_size;
    logwrite( function, message.str() );
#endif

    // Nothing to do if the cdsbuf is already the correct size.
    //
//  if ( this->cds_info.section_size == this->cdsbuf_size ) return;

    // Otherwise create a new set of cds ring buffers
    //
    this->cdsbuf_size = this->cds_info.section_size;
    message.str(""); message << "allocated " << std::dec << this->cdsbuf_size << " pixels for CDS ring buffer";
    for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
      if ( this->cds_ring.at(i) != nullptr ) { delete [] (T*)this->cds_ring.at(i); this->cds_ring.at(i)=nullptr; }
      this->cds_ring.at(i) = (T*) new T [ this->cds_info.section_size ]{};
      message << " " << std::dec << i << ":" << std::hex << (void*)this->cds_ring.at(i);
    }
    logwrite( function, message.str() );

    if ( this->coaddbuf != nullptr ) { delete [] (int32_t*)this->coaddbuf; this->coaddbuf=nullptr; }
    this->coaddbuf = (int32_t*) new int32_t [ this->cds_info.section_size ]{};

    if ( this->mcdsbuf_0 != nullptr ) delete [] (int32_t*)this->mcdsbuf_0;
    this->mcdsbuf_0 = (int32_t*) new int32_t [ this->cds_info.section_size ]{};

    if ( this->mcdsbuf_1 != nullptr ) delete [] (int32_t*)this->mcdsbuf_1;
    this->mcdsbuf_1 = (int32_t*) new int32_t [ this->cds_info.section_size ]{};

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] allocated " << this->cds_info.section_size
                             << " pixels for mcdsbuf_0 at " << std::hex << (void*)this->mcdsbuf_0
                             << " and mcdsbuf_1 at " << std::hex << (void*)this->mcdsbuf_1;
    logwrite( function, message.str() );
#endif

    return;
  }
  /***** Archon::Interface::alloc_cdsring *************************************/


  /***** Archon::Interface::alloc_workring ************************************/
  /**
   * @brief      allocate workspace memory for deinterlacing
   * @param[in]  buf, pointer to template type T
   * @return     pointer to the allocated space
   *
   * The actual allocation occurs in here, based on the template class pointer type.
   *
   */
  template <class T>
  void Interface::alloc_workring( T* buf ) {
    const std::string function("Archon::Interface::alloc_workring");
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] camera_info.section_size=" << this->camera_info.section_size;
    logwrite( function, message.str() );
#endif

    // Nothing to do if the workbuf is already the correct size.
    //
//  if ( this->camera_info.section_size == this->workbuf_size ) return;

    // Otherwise create a new set of work ring buffers
    //
    try {
      message.str(""); message << "allocating " << std::dec << this->camera_info.section_size << " pixels for work ring buffers";
      for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
        if ( this->work_ring.at(i) != nullptr ) {
          delete [] (T*)this->work_ring.at(i);
          this->work_ring.at(i) = nullptr;
        }
        this->work_ring.at(i) = (T*) new T [ this->camera_info.section_size ]{};
        this->workbuf_size = this->camera_info.section_size;
        message << " " << std::dec << i << ":" << std::hex << (void*)this->work_ring.at(i);
      }
      logwrite( function, message.str() );
    }
    catch ( std::out_of_range & ) {
      this->camera.log_error( function, "unable to address work ring buffer" );
    }
    return;
  }
  /***** Archon::Interface::alloc_workring ************************************/


  /***** Archon::Interface::free_workring *************************************/
  /**
   * @brief      clean up work ring buffer memory
   * @param[in]  buf, pointer to template type T
   *
   */
  template <class T>
  void Interface::free_workring( T* buf ) {
    const std::string function("Archon::Interface::free_workring");
    std::stringstream message;
    message.str(""); message << "freed work ring buffer  ";
    for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
      if ( this->work_ring.at(i) != nullptr ) {
        delete [] (T*)this->work_ring.at(i);
        message << " " << std::dec << i << ":" << std::hex << (void*)this->work_ring.at(i);
        this->work_ring.at(i) = nullptr;
      }
    }
    logwrite( function, message.str() );
    return;
  }
  /***** Archon::Interface::free_workring *************************************/


  /***** Archon::Interface::free_cdsring **************************************/
  /**
   * @brief      clean up cds ring buffer memory
   * @param[in]  buf, pointer to template type T
   *
   * This also frees the coadd buffer
   *
   */
  template <class T>
  void Interface::free_cdsring( T* buf ) {
    const std::string function("Archon::Interface::free_cdsring");
    std::stringstream message;
    message << "freed cds ring buffer    ";
    for ( int i=0; i<Archon::IMAGE_RING_BUFFER_SIZE; i++ ) {
      if ( this->cds_ring.at(i) != nullptr ) {
        delete [] (T*)this->cds_ring.at(i);
        message << " " << std::dec << i << ":" << std::hex << (void*)this->cds_ring.at(i);
        this->cds_ring.at(i) = nullptr;
      }
    }
    logwrite( function, message.str() );
    if ( this->coaddbuf != nullptr ) {
      delete [] (int32_t*)this->coaddbuf;
      this->coaddbuf=nullptr;
    }
    if ( this->mcdsbuf_0 != nullptr ) {
      delete [] (int32_t*)this->mcdsbuf_0;
      this->mcdsbuf_0=nullptr;
    }
    if ( this->mcdsbuf_1 != nullptr ) {
      delete [] (int32_t*)this->mcdsbuf_1;
      this->mcdsbuf_1=nullptr;
    }
    return;
  }
  /***** Archon::Interface::free_cdsring **************************************/


  /***** Archon::Interface::free_workbuf **************************************/
  /**
   * @brief      free (delete) memory allocated by alloc_workbuf
   * @param[in]  buf, pointer to template type T
   *
   * @todo       I think this is obsolete now that I switched to a ring buffer 2/14/23
   *
   * Must pass a pointer of the correct type because delete doesn't work on void.
   *
   */
  template <class T>
  void Interface::free_workbuf(T* buf) {
    const std::string function("Archon::Interface::free_workbuf");
    std::stringstream message;
    if (this->workbuf != nullptr) {
      delete [] (T*)this->workbuf;
      this->workbuf = nullptr;
      this->workbuf_size = 0;
      message << "deleted old deinterlacing buffer " << std::hex << (void*)this->workbuf;
      logwrite(function, message.str());
    }
  }
  /***** Archon::Interface::free_workbuf **************************************/


  /***** Archon::Interface::deinterlace ***************************************/
  /**
   * @brief      spawns the deinterlacing threads
   * @param[in]  _imbuf      pointer to buffer which contains the original image
   * @param[in]  _workbuf    pointer to buffer that contains the deinterlaced image
   * @param[in]  _ringcount  the current ring buffer to deinterlace
   * @return     T* pointer to workbuf
   *
   */
  template <class T>
  T* Interface::deinterlace( T* _imbuf, T* _workbuf, T* _cdsbuf, int _ringcount ) {
    debug( "DEINTERLACE_ENTRY" );
    const std::string function("Archon::Instrument::deinterlace");
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message << "[DEBUG] cds_info.section_size=" << this->cds_info.section_size << " sizeof(int32_t)=" << sizeof(int32_t)
            << " -> " << this->cds_info.section_size*sizeof(int32_t) << " bytes for "
            << " mcdsbuf_0=" << std::hex << (void*)mcdsbuf_0
            << " mcdsbuf_1=" << std::hex << (void*)mcdsbuf_1;
    logwrite( function, message.str() );
#endif

    // Zero the buffers that will be used to sum the MCDS baseline (mcdsbuf_0) and
    // signal (mcdsbuf_1) frames.
    //
    if ( this->mcdsbuf_0 != nullptr ) memset( this->mcdsbuf_0, 0, this->cds_info.section_size * sizeof(int32_t) );
    if ( this->mcdsbuf_1 != nullptr ) memset( this->mcdsbuf_1, 0, this->cds_info.section_size * sizeof(int32_t) );

    // Instantiate a DeInterlace class object,
    // which is constructed with the pointers need for the image and working buffers.
    // This object contains the functions needed for the deinterlacing,
    // which will get called by the threads created here.
    //
    DeInterlace<T> deinterlace( (T*)_imbuf,                            // pointer to buffer that contains the raw image
                                (T*)_workbuf,                          // pointer to buffer that contains the deinterlaced image
                                (T*)_cdsbuf,                           // pointer to buffer that contains the deinterlaced image
                                coaddbuf,                              // pointer to buffer to contain the coadded image
                                mcdsbuf_0,                             // pointer to buffer to contain the MCDS baseline sum (1st half)
                                mcdsbuf_1,                             // pointer to buffer to contain the MCDS signal sum (2nd half)
                                this->camera_info.iscds,
                                this->camera_info.nmcds,
                                this->camera_info.detector_pixels[0],  // cols
                                this->camera_info.detector_pixels[1],  // rows
                                this->camera_info.readout_type,        // selects type of deinterlacing
                                this->camera_info.imheight,            // frame_rows
                                this->camera_info.imwidth,             // frame_cols
                                this->camera_info.cubedepth            // depth
                              );

    {
#ifdef LOGLEVEL_DEBUG
    logwrite( function, "[DEBUG] spawning deinterlacing thread" );
    message.str(""); message << "[DEBUG] ringcount_in=" << _ringcount << " iscds=" << this->camera_info.iscds
                             << " this->camera_info.detector_pixels[0]=" << this->camera_info.detector_pixels[0]
                             << " this->camera_info.detector_pixels[1] * this->camera_info.axes[2]="
                             << this->camera_info.detector_pixels[1] * this->camera_info.axes[2]
                             << " readout_type=" << this->camera_info.readout_type;
    logwrite( function, message.str() );
#endif

    std::vector<std::thread> threads;
    std::thread( std::ref( Archon::Interface::dothread_deinterlace<T> ),
                 this,
                 std::ref( deinterlace ),                                           // reference to the DeInterlace object just created above
                 _ringcount                                                         // selects the ringbuffer to deinterlace
               ).detach();
    }

    // Wait for the ring buffer to be deinterlaced
#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] waiting on deinterlace ringcount " << _ringcount;
    logwrite( function, message.str() );
#endif
    {
    std::unique_lock<std::mutex> lk( this->deinter_mtx );
    while ( ! this->ringbuf_deinterlaced.at( _ringcount ) ) this->deinter_cv.wait( lk );
    }
#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] done waiting on deinterlace ringcount " << _ringcount;
    logwrite( function, message.str() );
#endif

    debug( "DEINTERLACE_EXIT" );
    return( (T*)this->workbuf );
  }
  /***** Archon::Interface::deinterlace ***************************************/


  /***** Archon::Interface::dothread_deinterlace ******************************/
  /**
   * @brief      this is run in a thread to do the deinterlacing
   * @param[in]  self          pointer to this-> (Archon::Interface object)
   * @param[in]  deinterlace   address of DeInterlace object
   * @param[in]  bufrows       number of rows in raw image buffer
   * @param[in]  ringcount_in  the current ring buffer to deinterlace
   *
   */
  template <class T> void Interface::dothread_deinterlace( Interface *self, DeInterlace<T> &deinterlace, int ringcount_in ) {
    debug( "DOTHREAD_DEINTERLACE_ENTRY ringcount="+std::to_string(ringcount_in) );
    const std::string function("Archon::Interface::dothread_deinterlace");
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] ringcount_in=" << ringcount_in
                             << " mex=" << ( self->camera.mex() ? "true" : "false" ); // << " info:" << deinterlace.info();
    logwrite(function, message.str());
#endif

    // Create appropriately-sized cv::Mat arrays for the reset and read frames in the DeInterlace object.
    // The deinterlace function will copy the appropriate frames into these objects.
    //
//  deinterlace.resetframe = cv::Mat( self->camera_info.imheight, self->camera_info.imwidth, CV_16U, cv::Scalar(0) );
//  deinterlace.readframe  = cv::Mat( self->camera_info.imheight, self->camera_info.imwidth, CV_16U, cv::Scalar(0) );

    // The DeInterlace object contains the actual de-interlacing functions
    //
    deinterlace.do_deinterlace();

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] deinterlace for ring " << ringcount_in << " is done -- notify the FITS writer";
    logwrite( function, message.str() );
#endif
    {
    std::unique_lock<std::mutex> lk( self->deinter_mtx );
    self->ringbuf_deinterlaced.at( ringcount_in )=true;
    }
    ++self->deinterlace_count;
    self->deinter_cv.notify_all();

    debug( "DOTHREAD_DEINTERLACE_EXIT ringcount="+std::to_string(ringcount_in) );
    return;
  }
  /***** Archon::Interface::dothread_deinterlace ******************************/


  /***** Archon::Interface::dothread_runmcdsproc ******************************/
  /**
   * @brief      
   * @param[in]  self  pointer to Archon::Interface object
   * @todo       TODO not sure this is needed anymore
   *
   */
  void Interface::dothread_runmcdsproc( Interface *self ) {
    const std::string function("Archon::Interface::dothread_runmcdsproc");
    std::stringstream message;
/***
    CPyInstance hInstance;
    CPyObject pName   = PyUnicode_FromString( "calcmcds.py" );
    CPyObject pModule = PyImport_Import( pName );
    CPyObject pFunc = PyObject_GetAttrString( pModule, "do_calc" );
***/
  }
  /***** Archon::Interface::dothread_runmcdsproc ******************************/


  /***** Archon::Interface::dothread_runcds ***********************************/
  /**
   * @brief      
   * @param[in]  self  pointer to Archon::Interface object
   * @todo       TODO not sure this is needed anymore
   *
   * This thread is spawned at the start of do_expose() if camera_info.iscds is true.
   * It then waits for a deinter_cv signal, which is notified by dothread_deinterlace().
   * dothread_deinterlace() is a thread spawned by deinterlace() which is called by
   * do_expose() after reading a frame buffer (or in this case, after reading a data
   * cube of frame buffers).
   *
   * Since this thread is spawned once for each exposure, the local cv::Mat coadd
   * image created here remains throughout the entire exposure, so this is the total
   * coadd for any and all frames collected for this exposure.
   *
   */
  void Interface::dothread_runcds( Interface *self ) {
    debug( "DOTHREAD_RUNCDS_ENTRY" );
    const std::string function("Archon::Interface::dothread_runcds");
    std::stringstream message;
    int deinterlace_count = self->deinterlace_count.load( std::memory_order_seq_cst );

//cv::namedWindow( "image", cv::WINDOW_AUTOSIZE );

    // Create a Mat image for the final MCDS coadd
    // Using smart pointers to automatically clean up.
    //
    std::unique_ptr<cv::Mat> coadd( new cv::Mat( cv::Mat::zeros( self->cds_info.imheight, self->cds_info.imwidth, CV_32S ) ) );

    std::unique_ptr<cv::Mat> diff( new cv::Mat( cv::Mat::zeros( self->cds_info.imheight, self->cds_info.imwidth, CV_32S ) ) );

    std::unique_ptr<cv::Mat> mcds_0(nullptr);
    std::unique_ptr<cv::Mat> mcds_1(nullptr);

    message << "waiting for CDS/MCDS frames: self->deinterlace_count.load()=" << deinterlace_count
            << " self->camera_info.nseq=" << self->camera_info.nseq;
    logwrite( function, message.str() );

    // Each count here is either a pair of CDS frames,
    // or a set of "nmcds" frames.
    //
    {
    std::unique_lock<std::mutex> lk( self->deinter_mtx );
    while ( ! self->camera.is_aborted() && ( deinterlace_count < self->camera_info.nseq ) ) {
      self->deinter_cv.wait( lk );
      deinterlace_count = self->deinterlace_count.load( std::memory_order_seq_cst );
      debug( "CDS_SUBTRACTION_START frame="+std::to_string(self->frame.bufframen[self->frame.index])+
             " deinterlace_count="+std::to_string(deinterlace_count) );
//    if ( self->camera.is_aborted() ) { self->deinterlace_count.store(self->camera_info.nseq); }
      message.str(""); message << "deinterlace_count=" << deinterlace_count;
      logwrite( function, message.str() );
      if ( self->cds_info.nmcds > 0 ) {
#ifdef LOGLEVEL_DEBUG
        logwrite( function, "[DEBUG] performing MCDS subtraction" );
#endif
        // Perform the CDS subtraction, sum of signal frames - sum of baseline frames, average,
	// then coadd the result. To do that, create Mat arrays from each of the MCDS buffers.
        //
        try {
          mcds_0.reset( new cv::Mat( self->cds_info.imheight, self->cds_info.imwidth, CV_32S, self->mcdsbuf_0 ) );
          mcds_1.reset( new cv::Mat( self->cds_info.imheight, self->cds_info.imwidth, CV_32S, self->mcdsbuf_1 ) );

          *diff   = *mcds_1 - *mcds_0;           // perform the subtraction, signal-baseline
          *diff  /= ( self->cds_info.nmcds/2 );  // average
          *coadd += *diff;                       // coadd here
        }
        catch ( const cv::Exception& ex ) {
          message.str(""); message << "ERROR OpenCV exception subtracting signal-baseline: " << ex.what();
          logwrite( function, message.str() );
//cv::destroyAllWindows();
          return;
        }
        catch ( const std::exception& ex ) {
          message.str(""); message << "ERROR std exception subtracting signal-baseline: " << ex.what();
          logwrite( function, message.str() );
//cv::destroyAllWindows();
          return;
        }
        catch ( ... ) {
          logwrite( function, "unknown exception subtracting signal-baseline" );
//cv::destroyAllWindows();
          return;
        }
      }
      debug( "CDS_SUBTRACTION_END frame="+std::to_string(self->frame.bufframen[self->frame.index])+
             " deinterlace_count="+std::to_string(deinterlace_count) );
    }
    }

    // Now that all frames have been completed, it's time to write the co-added image
    //
    long error=NO_ERROR;
    if ( ! self->camera.is_aborted() && self->camera_info.nmcds == 0 ) {
//logwrite(function,"dothread_runcds (a) calling orig xcds_file.write_image()");
//      error = self->xcds_file.write_image( self->coaddbuf, self->cds_info );
      logwrite( function, "[DEBUG] dothread_runcds (a) calling __file_cds->write_image" );
      self->__file_cds->write_image( self->coaddbuf,
                                     get_timestamp(),
                                     0,
                                     self->cds_info
                                   );
    }
    else if ( ! self->camera.is_aborted() ) {
#ifdef LOGLEVEL_DEBUG
        logwrite( function, "[DEBUG] copying MCDS coadd image to FITS buffer" );
#endif
      // Copy assembled image into the FITS buffer, this->coaddbuf
      //
      unsigned long index=0;
      for ( int row=0; row<self->cds_info.imheight; row++ ) {
        for ( int col=0; col<self->cds_info.imwidth; col++ ) {
          *( self->coaddbuf + index++ ) = static_cast<int32_t>(coadd->at<int32_t>(row, col));
        }
      }
      debug( "CDS_FILE_WRITE_START frame="+std::to_string(self->frame.bufframen[self->frame.index])+
             " deinterlace_count="+std::to_string(deinterlace_count) );
//logwrite(function,"dothread_runcds (b) calling orig xcds_file.write_image()");
//      error = self->xcds_file.write_image( self->coaddbuf, self->cds_info );
      logwrite( function, "[DEBUG] dothread_runcds (b) calling __file_cds->write_image" );
      self->__file_cds->write_image( self->coaddbuf,
                                     get_timestamp(),
                                     0,
                                     self->cds_info
                                   );
      debug( "CDS_FILE_WRITE_END frame="+std::to_string(self->frame.bufframen[self->frame.index])+
             " deinterlace_count="+std::to_string(deinterlace_count) );
    }
    if ( error != NO_ERROR ) logwrite( function, "ERROR writing coadd image to disk" );
    if ( self->camera.is_aborted() ) logwrite( function, "closing aborted coadd image" );
    self->cds_info.exposure_aborted = self->camera.is_aborted();
//  self->xcds_file.close_file(  (self->camera.writekeys_when=="after"?true:false), self->cds_info );

//cv::destroyAllWindows();
    logwrite( function, "exiting CDS thread" );
      debug( "DOTHREAD_RUNCDS_EXIT frame="+std::to_string(self->frame.bufframen[self->frame.index])+
             " deinterlace_count="+std::to_string(deinterlace_count) );
    return;
  }
  /***** Archon::Interface::dothread_runcds ***********************************/


  /***** Archon::Interface::dothread_openfits *********************************/
  /**
   * @brief      this is run in a thread to open a fits file for flat (non-mex) files only
   * @param[in]  self  pointer to Archon::Interface object
   *
   */
  void Interface::dothread_openfits( Interface *self ) {
    debug( "DOTHREAD_OPENFITS_ENTRY" );
    const std::string function("Archon::Interface::dothread_openfits");
    std::stringstream message;
    long error = NO_ERROR;

    self->camera_info.start_time = get_timestamp();                    // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss

    self->camera.set_fitstime( self->camera_info.start_time );         // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename

    error = self->camera.get_fitsname( self->camera_info.fits_name );  // Assemble the FITS filename

    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR: couldn't validate fits filename" );
      self->openfits_error.store( true, std::memory_order_seq_cst );
      return;
    }
    self->add_filename_key();                                          // add filename to system keys database

    // reset the extension number and open the fits file
    //
#ifdef LOGLEVEL_DEBUG
    logwrite( function, "[DEBUG] reset extension=0 and opening new fits file" );
#endif
    self->camera_info.extension.store(0);
    self->camera_info.writekeys_before = (self->camera.writekeys_when=="before"?true:false);
//  error = self->xfits_file.open_file( (self->camera.writekeys_when=="before"?true:false), self->camera_info );
    self->__fits_file = std::make_unique<FITS_file<uint16_t>>(false);
    if ( error != NO_ERROR ) {
      self->camera.log_error( function, "couldn't open fits file" );
      self->openfits_error.store( true, std::memory_order_seq_cst );
      return;
    }
    debug( "DOTHREAD_OPENFITS_EXIT" );
    return;
  }
  /***** Archon::Interface::dothread_openfits *********************************/


  /***** Archon::Interface::dothread_start_deinterlace ************************/
  /**
   * @brief      calls the appropriate deinterlacer based on camera_info.datatype
   * @param[in]  self          pointer to Archon::Interface object
   * @param[in]  ringcount_in  the current ring buffer to deinterlace
   *
   */
  void Interface::dothread_start_deinterlace( Interface *self, int ringcount_in ) {
    debug( "DOTHREAD_START_DEINTERLACE_ENTRY ring="+std::to_string(ringcount_in) );
    const std::string function("Archon::Interface::dothread_start_deinterlace");
    std::stringstream message;

    // If this ring buffer is marked as locked then that means a thread is currently reading data into it,
    // which means we should not be here trying to deinterlace it. Either got here too fast or the read
    // is taking too long.
    //
    bool ringlock = (*self->ringlock.at( ringcount_in )).load( std::memory_order_seq_cst );
    if ( ringlock ) {
      message.str(""); message << "RING BUFFER OVERFLOW: ring buffer " << ringcount_in << " is locked for writing";
      self->camera.log_error( function, message.str() );
      return;
    }

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] starting deinterlace for image_ring[" << std::dec << ringcount_in << "]="
                             << std::hex << (void*)self->image_ring.at(ringcount_in)
                             << " into work_ring[" << std::dec << ringcount_in << "]=" << std::hex << (void*)self->work_ring.at(ringcount_in);
    logwrite( function, message.str() );
#endif

    // Call the appropriate deinterlacer here
    //
    switch ( self->camera_info.datatype ) {
      case USHORT_IMG: {
        self->deinterlace( (uint16_t *)self->image_ring.at(ringcount_in),
                           (uint16_t *)self->work_ring.at(ringcount_in),
                           (uint16_t *)self->cds_ring.at(ringcount_in),
                           ringcount_in );
        break;
      }
      case SHORT_IMG: {
        self->deinterlace( (int16_t *)self->image_ring.at(ringcount_in),
                           (int16_t *)self->work_ring.at(ringcount_in),
                           (int16_t *)self->cds_ring.at(ringcount_in),
                           ringcount_in );
        break;
      }
      case FLOAT_IMG: {
        self->deinterlace( (uint32_t *)self->image_ring.at(ringcount_in),
                           (uint32_t *)self->work_ring.at(ringcount_in),
                           (uint32_t *)self->cds_ring.at(ringcount_in),
                           ringcount_in );
        break;
      }
      default:
        message.str(""); message << "unknown datatype " << self->camera_info.datatype;
        self->camera.log_error( function, message.str() );
        return;
        break;
    }
    debug( "DOTHREAD_START_DEINTERLACE_EXIT ring="+std::to_string(ringcount_in) );
    return;
  }
  /***** Archon::Interface::dothread_start_deinterlace ************************/


  /***** Archon::Interface::dothread_writeframe *******************************/
  /**
   * @brief      this is run in a thread to write a frame after it is deinterlaced
   * @param[in]  self          pointer to Archon::Interface object
   * @param[in]  ringcount_in  the current ring buffer to write
   *
   * This thread will wait for the ringbuffer at ringcount_in to be deinterlaced,
   * then it will write the frame.
   *
   * *** THIS IS THE ONLY CALL TO write_frame() USED BY NIRC2 ***
   *
   */
  void Interface::dothread_writeframe( Interface *self, int ringcount_in ) {
    debug( "DOTHREAD_WRITEFRAME_ENTRY ring="+std::to_string(ringcount_in) );
    const std::string function("Archon::Interface::dothread_writeframe");
    std::stringstream message;

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] waiting for ringbuf_deinterlaced[" << ringcount_in << "]";
    logwrite( function, message.str() );
#endif

    // Wait for the ring buffer to be deinterlaced
    {
    std::unique_lock<std::mutex> lk( self->deinter_mtx );
    while ( /* ! self->camera.is_aborted() and*/ ! self->ringbuf_deinterlaced.at( ringcount_in ) ) self->deinter_cv.wait( lk ); //DDSH TODO check this
    }
    debug( "DOTHREAD_WRITEFRAME_START ring="+std::to_string(ringcount_in) );

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] after the lock ringbuf_deinterlaced[" << ringcount_in << "]=" << self->ringbuf_deinterlaced.at(ringcount_in) 
                             << " calling write_frame(" << ringcount_in << ")";
    logwrite( function, message.str() );
#endif

    // Write the frame
    //
    self->write_frame( ringcount_in );

    if ( self->camera.mex() ) ++self->write_frame_count;

#ifdef LOGLEVEL_DEBUG
    int wfc = self->write_frame_count.load( std::memory_order_seq_cst );
    message.str(""); message << "[DEBUG] write_frame(" << ringcount_in << ") is done. write_frame_count=" << wfc;
    logwrite( function, message.str() );
#endif

    debug( "DOTHREAD_WRITEFRAME_EXIT ring="+std::to_string(ringcount_in) );
    return;
  }
  /***** Archon::Interface::dothread_writeframe *******************************/

}
