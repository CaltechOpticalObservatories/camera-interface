#include "emulator-archon.h"
namespace Archon {

  // Archon::Interface constructor
  //
  Interface::Interface() {
    struct timespec ts;     // container for the time
    if ( clock_gettime( CLOCK_REALTIME, &ts ) != 0 ) {
      this->init_time = 0;
    }
    else {
      this->init_time = 1E8 * ( ts.tv_sec + (ts.tv_nsec / 1E9) );
    }

    this->modtype.resize( nmods );
    this->modversion.resize( nmods );
    this->poweron = false;
    this->bigbuf = false;

    this->image.framen = 0;
    this->image.activebufs = -1;
    this->image.taplines = -1;
    this->image.linecount = -1;
    this->image.pixelcount = -1;
    this->image.readtime = -1;
    this->image.exptime = 0;
    this->image.exposure_factor = 1000;  // default is msec

    this->frame.index = 0;
    this->frame.frame = 0;
    this->frame.rbuf = 0;
    this->frame.wbuf = 0;
    this->frame.timer = "";

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
//  this->frame.bufreatimestamp.resize( Archon::nbufs );  // TODO future
//  this->frame.buffeatimestamp.resize( Archon::nbufs );
//  this->frame.bufrebtimestamp.resize( Archon::nbufs );
//  this->frame.buffebtimestamp.resize( Archon::nbufs );
  }

  // Archon::Interface deconstructor
  //
  Interface::~Interface() {
  }

  /**************** Interface::configure_controller ***************************/
  /**
   * @fn     configure_controller
   * @brief  get configuration parameters from .cfg file
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * Called at startup to read the configuration file.
   *
   */
  long Interface::configure_controller() {
    std::string function = "(Archon::Interface::configure_controller) ";

    // loop through the entries in the configuration file, stored in config class
    //
    for ( int entry=0; entry < this->config.n_entries; entry++ ) {

      try {
        if ( config.param.at(entry).compare(0, 15, "EMULATOR_SYSTEM")==0 ) {
          this->systemfile = config.arg.at(entry);
        }
        if ( config.param.at(entry).compare(0, 12, "READOUT_TIME")==0 ) {
          this->image.readtime = std::stoi( config.arg.at(entry) );
        }
        if ( config.param.at(entry).compare(0, 12, "EXPOSE_PARAM")==0) {
          this->exposeparam = config.arg[entry];
        }
      }
      catch( std::invalid_argument & ) {
        std::cerr << function << "ERROR: invalid argument parsing entry " << entry << " of " << this->config.n_entries << "\n";
        return( ERROR );
      }
      catch( std::out_of_range & ) {
        std::cerr << function << "ERROR: value out of range parsing entry " << entry << " of " << this->config.n_entries << "\n";
        return( ERROR );
      }
      catch( ... ) {
        std::cerr << function << "unknown error parsing entry " << entry << " of " << this->config.n_entries << "\n";
        return( ERROR );
      }
    }

    return( NO_ERROR );
  }
  /**************** Interface::configure_controller ***************************/


  /**************** Interface::system_report **********************************/
  /**
   * @fn     system_report
   * @brief  handles the incoming SYSTEM command
   * @param  buf, incoming command string
   * @param  &retstring, reference to string for return values
   * @return ERROR or NO_ERROR
   *
   * This reads the emulated system information from a file specified in
   * the configuration file by EMULATOR_SYSTEM.
   *
   */
  long Interface::system_report(std::string buf, std::string &retstring) {
    std::string function = "(Archon::Interface::system_report) ";
    std::fstream filestream;
    std::string line;
    std::vector<std::string> tokens;

    // need system file
    //
    if ( this->systemfile.empty() ) {
      std::cerr << function << "ERROR: missing EMULATOR_SYSTEM from configuration file\n";
      return( ERROR );
    }

    // try to open the file
    //
    try {
      filestream.open( this->systemfile, std::ios_base::in );
    }
    catch(...) {
      std::cerr << function << "ERROR: opening system file: " << this->systemfile << ": " << std::strerror(errno) << "\n";
      return( ERROR );
    }

    if ( ! filestream.is_open() || ! filestream.good() ) {
      std::cerr << function << " ERROR: system file: " << this->systemfile << " not open\n";
      return( ERROR );
    }

    while ( getline( filestream, line ) ) {

      if ( line == "[SYSTEM]" ) continue;

      retstring += line + " ";

      Tokenize( line, tokens, "_=" );
      if ( tokens.size() != 3 ) continue;

      int module=0;
      std::string version="";

      // get the type of each module from MODn_TYPE
      //
      try {

        if ( tokens.at(0).compare( 0, 9, "BACKPLANE" ) == 0 ) {
          if ( tokens.at(1) == "VERSION" ) this->backplaneversion = tokens.at(2);
          continue;
        }

        if ( ( tokens.at(0).compare(0,3,"MOD")==0 ) && ( tokens.at(1)=="TYPE" ) ) {
          module = std::stoi( tokens.at(0).substr(3) );
          if ( ( module > 0 ) && ( module <= nmods ) ) {
            this->modtype.at( module-1 ) = std::stoi( tokens.at(2) );
          }
          else {
            std::cerr << function << "ERROR: module " << module << " outside range {0:" << nmods << "}\n";
            return( ERROR );
          }
        }

        // get the module version
        //
        if ( ( tokens.at(0).compare(0,3,"MOD")==0 ) && ( tokens.at(1) == "VERSION" ) ) {
          module = std::stoi( tokens.at(0).substr(3) );
          if ( ( module > 0 ) && ( module <= nmods ) ) {
            this->modversion.at( module-1 ) = tokens.at(2);
          }
          else {
            std::cerr << function << "ERROR: module " << module << " outside range {0:" << nmods << "}\n";
            return( ERROR );
          }
        }
        else {
          continue;
        }
      }
      catch( std::invalid_argument & ) {
        std::cerr << function << "ERROR: invalid argument, unable to convert module or type\n";
        return( ERROR );
      }
      catch( std::out_of_range & ) {
        std::cerr << function << "ERROR: value out of range, unable to convert module or type\n";
        return( ERROR );
      }
      catch( ... ) {
        std::cerr << function << "unknown error converting module or type\n";
        return( ERROR );
      }

    }

    return( NO_ERROR );
  }
  /**************** Interface::system_report **********************************/


  /**************** Interface::status_report ****8*****************************/
  /**
   * @fn     status_report
   * @brief  
   * @param  
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::status_report(std::string &retstring) {
    std::string function = "(Archon::Interface::status_report) ";
    std::stringstream statstr;

    statstr << "VALID="          <<  1            << " "
            << "COUNT="          <<  1            << " "
            << "LOG="            <<  0            << " "
            << "POWER="          << this->poweron << " "
            << "POWERGOOD="      <<  1            << " "
            << "OVERHEAT="       <<  0            << " "
            << "BACKPLANE_TEMP=" << 40            << " "
            << "P2V5_V="         << 2.5           << " "
            << "P2V5_I="         <<  0            << " "
            << "P5V_V="          << 5.0           << " "
            << "P5V_I="          <<  0            << " "
            << "P6V_V="          << 6.0           << " "
            << "P6V_I="          <<  0            << " "
            << "N6V_V="          << -6.0          << " "
            << "N6V_I="          <<  0            << " "
            << "P17V_V="         << 17.0          << " "
            << "P17V_I="         <<  0            << " "
            << "N17V_V="         << -17.0         << " "
            << "N17V_I="         <<  0            << " "
            << "P35V_V="         << 35.0          << " "
            << "P35V_I="         <<  0            << " "
            << "N35V_V="         << -35.0         << " "
            << "N35V_I="         <<  0            << " "
            << "P100V_V="        << 100.0         << " "
            << "P100V_I="        <<  0            << " "
            << "N100V_V="        << -100.0        << " "
            << "N100V_I="        <<  0            << " "
            << "USER_V="         <<  0            << " "
            << "USER_I="         <<  0            << " "
            << "HEATER_V="       <<  0            << " "
            << "HEATER_I="       <<  0            << " "
            << "FANTACH="        <<  0
            ;
    retstring = statstr.str();
    return( NO_ERROR );
  }
  /**************** Interface::status_report ****8*****************************/


  /**************** Interface::timer_report *****8*****************************/
  /**
   * @fn     timer_report
   * @brief  returns the "Archon TIMER"
   * @param  &retstring, reference to string for return value
   * @return ERROR or NO_ERROR
   *
   * Returns the Archon TIMER which is a 64 bit hex representation of
   * the Archon time, counted in 10 nsec ticks.
   *
   */
  long Interface::timer_report(std::string &retstring) {
    std::string function = "(Archon::Interface::timer_report) ";
    struct timespec data;     // container for the time
    unsigned long long tm;    // 64 bit int time in 10 nsec
    std::stringstream tmstr;  // padded hex representation

    if ( clock_gettime( CLOCK_REALTIME, &data ) != 0 ) return( ERROR );
    tm = 1E8 * ( data.tv_sec + (data.tv_nsec / 1E9) ) - this->init_time;
    tmstr << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << tm;
    retstring = tmstr.str();
    return( NO_ERROR );
  }
  unsigned long Interface::get_timer() {
    struct timespec data;     // container for the time
    unsigned long long tm;    // 64 bit int time in 10 nsec
    if ( clock_gettime( CLOCK_REALTIME, &data ) != 0 ) return( 0 );
    tm = 1E8 * ( data.tv_sec + (data.tv_nsec / 1E9) ) - this->init_time;
    return( tm );
  }
  /**************** Interface::timer_report *****8*****************************/


  /**************** Interface::frame_report *****8*****************************/
  /**
   * @fn     frame_report
   * @brief  
   * @param  
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::frame_report(std::string &retstring) {
    std::string function = "(Archon::Interface::frame_report) ";
    std::string timenow;
    std::stringstream framestr;

    if ( this->image.activebufs == -1 ) {
      std::cerr << function << "ERROR: activebufs undefined. Check that an ACF was loaded and that it contains BIGBUF=x\n";
      return( ERROR );
    }

    this->timer_report( this->frame.timer );

    framestr << "TIMER=" << this->frame.timer << " "
             << "RBUF="  << this->frame.rbuf  << " "
             << "WBUF="  << this->frame.wbuf  << " ";

    int bufn;
    try {
      for ( bufn = 0; bufn < this->image.activebufs; bufn++ ) {
        framestr << "BUF" << bufn+1 << "SAMPLE="       << this->frame.bufsample.at(bufn)      << " "
                 << "BUF" << bufn+1 << "COMPLETE="     << this->frame.bufcomplete.at(bufn)    << " "
                 << "BUF" << bufn+1 << "MODE="         << this->frame.bufmode.at(bufn)        << " "
                 << "BUF" << bufn+1 << "BASE="         << this->frame.bufbase.at(bufn)        << " "
                 << "BUF" << bufn+1 << "FRAME="        << this->frame.bufframen.at(bufn)      << " "
                 << "BUF" << bufn+1 << "WIDTH="        << this->frame.bufwidth.at(bufn)       << " "
                 << "BUF" << bufn+1 << "HEIGHT="       << this->frame.bufheight.at(bufn)      << " "
                 << "BUF" << bufn+1 << "PIXELS="       << this->frame.bufpixels.at(bufn)      << " "
                 << "BUF" << bufn+1 << "LINES="        << this->frame.buflines.at(bufn)       << " "
                 << "BUF" << bufn+1 << "RAWBLOCKS="    << this->frame.bufrawblocks.at(bufn)   << " "
                 << "BUF" << bufn+1 << "RAWLINES="     << this->frame.bufrawlines.at(bufn)    << " "
                 << "BUF" << bufn+1 << "RAWOFFSET="    << this->frame.bufrawoffset.at(bufn)   << " "
                 << "BUF" << bufn+1 << "TIMESTAMP="    << this->frame.buftimestamp.at(bufn)   << " "
                 << "BUF" << bufn+1 << "RETIMESTAMP="  << this->frame.bufretimestamp.at(bufn) << " "
                 << "BUF" << bufn+1 << "FETIMESTAMP="  << this->frame.buffetimestamp.at(bufn) << " "
//               << "BUF" << bufn+1 << "REATIMESTAMP=" << " "
//               << "BUF" << bufn+1 << "FEATIMESTAMP=" << " "
//               << "BUF" << bufn+1 << "REBTIMESTAMP=" << " "
//               << "BUF" << bufn+1 << "FEBTIMESTAMP=" << " "
                 ;
      }
    }
    catch( std::invalid_argument & ) {
      std::cerr << function << "ERROR: invalid buffer number " << bufn+1 << "\n";
      return( ERROR );
    }
    catch( std::out_of_range & ) {
      std::cerr << function << "ERROR: buffer number " << bufn+1 << " out of range {1:" << this->image.activebufs << "}\n";
      return( ERROR );
    }
    catch( ... ) {
      std::cerr << function << "unknown error accessing buffer number " << bufn+1 << "\n";
      return( ERROR );
    }

    retstring = framestr.str();
    retstring = retstring.erase(retstring.find_last_not_of(" ")+1); // remove trailing space added because of for loop

    return( NO_ERROR );
  }
  /**************** Interface::frame_report *****8*****************************/


  /**************** Interface:fetch_data **************************************/
  /**
   * @fn     fetch_data
   * @brief  
   * @param  cmd, incoming command string
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::fetch_data(std::string ref, std::string cmd, Network::TcpSocket &sock) {
    std::string function = "(Archon::Interface::fetch_data) ";
    unsigned int reqblocks;  //!< number of requested blocks, from the FETCH command
    unsigned int block;      //!< block counter
    size_t byteswritten;     //!< bytes written for this block
    int totalbyteswritten;   //!< total bytes written for this image
    size_t towrite=0;        //!< remaining bytes to write for this block
    char* image_data=NULL;

    if ( cmd.length() != 21 ) {             // must be "FETCHxxxxxxxxyyyyyyyy", 21 chars
      std::cerr << function << "ERROR: expecting form FETCHxxxxxxxxyyyyyyyy but got \"" << cmd << "\"\n";
      return( ERROR );
    }

    try {
      std::stringstream hexblocks;
      hexblocks << std::hex << "0x" << cmd.substr(13);
      hexblocks >> reqblocks;
    }
    catch( std::invalid_argument & ) {
      std::cerr << function << "ERROR: invalid argument parsing " << cmd << "\n";
      return( ERROR );
    }
    catch( std::out_of_range & ) {
      std::cerr << function << "ERROR: value out of range parsing " << cmd << "\n";
      return( ERROR );
    }
    catch( ... ) {
      std::cerr << function << "unknown error parsing " << cmd << "\n";
      return( ERROR );
    }

    image_data = new char[reqblocks * BLOCK_LEN];

    std::srand( time( NULL ) );
    for ( unsigned int i=0; i<(reqblocks*BLOCK_LEN)/2; i+=10 ) {
      image_data[i] = rand() % 40000 + 30000;
    }

    std::string header = "<" + ref + ":";
    totalbyteswritten = 0;

    std::cerr << function << "host requested " << std::dec << reqblocks << " (0x" << std::hex << reqblocks << ") blocks \n";

    std::cerr << function << "writing bytes: ";

    for ( block = 0; block < reqblocks; block++ ) {
      sock.Write(header);
      byteswritten = 0;
      do {
        int retval=0;
        towrite = BLOCK_LEN - byteswritten;
        if ( ( retval = sock.Write(image_data, towrite) ) > 0 ) {
          byteswritten += retval;
          totalbyteswritten += retval;
          std::cerr << std::dec << std::setw(10) << totalbyteswritten << "\b\b\b\b\b\b\b\b\b\b";
        }
      } while ( byteswritten < BLOCK_LEN );
    }
    std::cerr << std::dec << std::setw(10) << totalbyteswritten << " complete\n";

    delete[] image_data;

    return( NO_ERROR );
  }
  /**************** Interface:fetch_data **************************************/


  /**************** Interface:wconfig *****************************************/
  /**
   * @fn     wconfig
   * @brief  handles the incoming WCONFIG command
   * @param  buf, incoming command string
   * @return ERROR or NO_ERROR
   *
   * Writes to emulated configuration memory, which is just an STL map
   * indexed by line number, so that lookups can be performed later.
   *
   */
  long Interface::wconfig(std::string buf) {
    std::string function = "(Archon::Interface::wconfig) ";
    std::string line;
    std::string linenumber;

    // check for minimum length or absence of an equal sign
    //
    if ( ( buf.length() < 14 ) ||                     // minimum string is "WCONFIGxxxxT=T", 14 chars
         ( buf.find("=") == std::string::npos ) ) {   // must have equal sign
      std::cerr << function << "ERROR: expecting form WCONFIGxxxxT=T but got \"" << buf << "\"\n";
      return( ERROR );
    }

    try {
      linenumber = buf.substr(7, 4);   // extract the line number (keep as string)
      line       = buf.substr(11);     // everything else

      // If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...
      //
      if ( ( line.compare(0, 11, "PARAMETERS=" ) != 0 ) && // not the PARAMETERS=xx line
           ( line.compare(0,  9, "PARAMETER"  ) == 0 ) ) { // but must start with "PARAMETER"

        std::vector<std::string> tokens;
        Tokenize( line, tokens, "=" );                     // separate into PARAMETERn, ParameterName, value tokens

        if ( tokens.size() != 3 ) {                        // malformed line
          std::cerr << function << "ERROR: expected 3 tokens but got \"" << line << "\"\n";
          return( ERROR );
        }

        std::stringstream paramnamevalue;
        paramnamevalue << tokens.at(1) << "=" << tokens.at(2);  // reassmeble ParameterName=value string

        // build an STL map "configmap" indexed on linenumber because that's what Archon uses
        //
        this->configmap[ linenumber ].line  = linenumber;            // configuration line number
        this->configmap[ linenumber ].key   = tokens.at(0);          // configuration key
        this->configmap[ linenumber ].value = paramnamevalue.str();  // configuration value for PARAMETERn

//      std::cerr << function << buf << " <<< stored parameter in configmap[ " << linenumber 
//                << " ].key=" << this->configmap[ linenumber ].key
//                << " .value=" << this->configmap[ linenumber ].value
//                << "\n";

        // build an STL map "parammap" indexed on ParameterName so that FASTPREPPARAM can lookup by the actual name
        //
        this->parammap[ tokens.at(1) ].key   = tokens.at(0);         // PARAMETERn
        this->parammap[ tokens.at(1) ].name  = tokens.at(1);         // ParameterName
        this->parammap[ tokens.at(1) ].value = tokens.at(2);         // value
        this->parammap[ tokens.at(1) ].line  = linenumber;           // line number

      }

      // ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
      // to be indexed by the key. Some lines may be equal to blank, e.g. "CONSTANTx=" so that
      // only one token is made
      //
      else {
        std::vector<std::string> tokens;
        std::string key, value;
        // Tokenize will return a size=1 even if there are no delimiters,
        // so work around this by first checking for delimiters
        // before calling Tokenize.
        //
        if (line.find_first_of("=", 0) == std::string::npos) {
          return( ERROR );
        }
        Tokenize(line, tokens, "=");                            // separate into KEY, VALUE tokens
        if (tokens.size() == 0) {
          return( ERROR );                                      // nothing to do here if no tokens (ie no "=")
        }
        if (tokens.size() > 0 ) {                               // at least one token is the key
          key   = tokens.at(0);                                 // KEY
          value = "";                                           // VALUE can be empty (e.g. labels not required)
          this->configmap[ linenumber ].line  = linenumber;
          this->configmap[ linenumber ].key   = key;
          this->configmap[ linenumber ].value = value;
        }
        if (tokens.size() > 1 ) {                               // if a second token then that's the value
          value = tokens.at(1);                                 // VALUE (there is a second token)
          this->configmap[ linenumber ].value = tokens.at(1);
        }

        // Some keywords in this category are used to assign certain class variables
        //
        if ( key == "BIGBUF" ) {
          this->bigbuf = ( std::stoi(value)==1 ? true : false );
          this->image.activebufs = ( this->bigbuf ? 2 : 3 );
          this->frame.bufbase.at(0) = 0xA0000000;
          if ( this->bigbuf ) {
            this->frame.bufbase.at(1) = 0xD0000000;
          }
          else {
            this->frame.bufbase.at(1) = 0xC0000000;
            this->frame.bufbase.at(2) = 0xE0000000;
          }
        }

        else if ( key == "TAPLINES" ) {
          this->image.taplines = std::stoi(value);
        }

        else if ( key == "PIXELCOUNT" ) {
          this->image.pixelcount = std::stoi(value);
        }

        else if ( key == "LINECOUNT" ) {
          this->image.linecount = std::stoi(value);
        }

      } // end else
    }
    catch( std::invalid_argument & ) {
      std::cerr << function << "ERROR: invalid argument parsing line: " << line << "\n";
      return( ERROR );
    }
    catch( std::out_of_range & ) {
      std::cerr << function << "ERROR: value out of range parsing line: " << line << "\n";
      return( ERROR );
    }
    catch( ... ) {
      std::cerr << function << "unknown error parsing line: " << line << "\n";
      return( ERROR );
    }

    return( NO_ERROR );
  }
  /**************** Interface:wconfig *****************************************/


  /**************** Interface::rconfig ****************************************/
  /**
   * @fn     rconfig
   * @brief  handles the incoming RCONFIG command
   * @param  buf, incoming command string
   * @param  &retstring, reference to string for return values
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::rconfig(std::string buf, std::string &retstring) {
    std::string function = "(Archon::Interface::rconfig) ";
    std::string linenumber;

    if ( buf.length() != 11 ) {        // check for minimum length "RCONFIGxxxx", 11 chars
      return( ERROR );
    }

    try {
      std::stringstream retss;
      linenumber = buf.substr(7, 4);   // extract the line number
      if ( this->configmap.find( linenumber) != this->configmap.end() ) {
        retss << this->configmap[ linenumber ].key;
        retss << "=";
        retss << this->configmap[ linenumber ].value;
        retstring = retss.str();
      }
      else {
        std::cerr << function << "ERROR: line " << linenumber << " not found in configuration memory\n";
        return( ERROR );
      }
    }
    catch( std::invalid_argument & ) {
      std::cerr << function << "ERROR: invalid argument\n";
      return( ERROR );
    }
    catch( std::out_of_range & ) {
      std::cerr << function << "ERROR: value out of range\n";
      return( ERROR );
    }
    catch( ... ) {
      std::cerr << function << "unknown error\n";
      return( ERROR );
    }
    return( NO_ERROR );
  }
  /**************** Interface::rconfig ****************************************/


  /**************** Interface::write_parameter ********************************/
  /**
   * @fn     write_parameter
   * @brief  writes parameter to emulated configuration memory
   * @param  buf
   * @return ERROR or NO_ERROR
   *
   * "buf" contains the space-delimited string "<Paramname> <value>"
   * where <value> is to be assigned to the parameter <Paramname>.
   *
   */
  long Interface::write_parameter(std::string buf) {
    std::string function = "(Archon::Interface::write_parameter) ";
    std::vector<std::string> tokens;
    std::string key="";
    std::string value="";
    std::string line="";

    Tokenize( buf, tokens, " " );

    // must have two tokens only, <param> <value>
    //
    if ( tokens.size() != 2 ) {
      std::cerr << function << "ERROR: expected <Paramname> <value> but received " << buf << "\n";
      return( ERROR );
    }

    // assign the key ("paramname") and value ("value") from the two tokens
    //
    try {
      key =   tokens.at(0);
      value = tokens.at(1);

      // When an exposure is started there will be a command to write
      // a non-zero number to the exposeparam. Catch that here and
      // start an exposure thread.
      //
      if ( ( key == this->exposeparam ) && ( std::stoi( value ) > 0 ) ) {
        int numexpose = std::stoi( value );
        // spawn a thread to mimic readout and create the data
        std::thread(dothread_expose, std::ref(this->frame), std::ref(this->image), numexpose).detach();
      }

      // Catch the write to exptime and save the value in a class variable.
      //
      if ( key == "exptime" ) {
        this->image.exptime = std::stoi( value );
        std::cerr << function << "exptime = " << this->image.exptime << ( this->image.exposure_factor == 1 ? " sec" : " msec" ) << "\n";
      }

      // Catch the write to longexposure which affects the class variable exposure_factor
      // exposure_factor=1000 when longexposure=0 (msec)
      // exposure_factor=1    when longexposure=1 (sec)
      //
      if ( key == "longexposure" ) {
        int longexposure = std::stoi( value );
        this->image.exposure_factor = ( longexposure == 1 ? 1 : 1000 );
        std::cerr << function << "exptime = " << this->image.exptime << ( this->image.exposure_factor == 1 ? " sec" : " msec" ) << "\n";
      }
    }
    catch( std::out_of_range & ) {
      std::cerr << function << "ERROR: token value out of range, unable to extract key, value pair\n";
      return( ERROR );
    }
    catch( ... ) {
      std::cerr << function << "unknown error extracting key, value pair\n";
      return( ERROR );
    }

    if ( key.empty() || value.empty() ) {   // should be impossible
      std::cerr << function << "ERROR: key or value cannot be empty\n";
      return( ERROR );
    }

    else

    // Locate the parameter name ("key") in the parammap
    // in order to get the line number.
    //
    if ( this->parammap.find( key ) == this->parammap.end() ) {
      std::cerr << function << "ERROR: " << key << " not found in parammap\n";
      return( ERROR );
    }

    // Assign the new value to the configmap (this is the "configuration memory").
    //
    else {
      line = this->parammap[ key ].line;         // line number is stored in parammap
      this->configmap[ line ].value = value;     // configmap is indexed by line number
      this->parammap[ key ].value = value;       //TODO needed??
    }

    return( NO_ERROR );
  }
  /**************** Interface::write_parameter ********************************/


  /**************** Interface::dothread_expose ********************************/
  /**
   * @fn     dothread_expose
   * @brief  
   * @param  numexpose
   * @return none
   *
   */
  void Interface::dothread_expose(frame_data_t &frame, image_t &image, int numexpose) {
    std::string function = "(Archon::Interface::dothread_expose) ";

    // should be impossible
    //
    if ( numexpose == 0 ) {
      std::cerr << function << "ERROR: need non-zero number of exposures\n";
      return;
    }

    for ( int num = 0; num < numexpose; num++ ) {

      // emulate an exposure delay
      //
      if ( image.exptime > 0 ) {
        double time_now   = get_clock_time();  // in seconds
        double time_start = time_now;
        double time_end   = time_start + image.exptime/image.exposure_factor;
        double progress   = 0;
        double elapsed    = 0;
        std::cerr << function << "exposure progress: ";
        while ( ( time_now - (image.exptime/image.exposure_factor + time_start) < 0 ) ) {
          usleep( 1000 );
          time_now = get_clock_time();
          elapsed = 1000. * (get_clock_time() - time_start);
          progress = elapsed / (1000.*(time_end - time_start));
          if ( progress < 0 || progress > 1 ) progress = 1;
          std::cerr << std::setw(3) << (int)(100*progress) << "\%\b\b\b\b";
        }
        std::cerr << "100\%\n";
      }

      // frame.frame is the 1-based frame buffer number to write to now
      // and frame.index is the 0-based index of this frame buffer number
      // increment each time
      // cycle back to 1 if greater than the number of active buffers
      //
      frame.frame++;
      if ( frame.frame > image.activebufs ) frame.frame = 1;
      frame.index = frame.frame - 1;

      // compute line time as 90% of the read time, rounded down to nearest 100 usec
      //
      int linetime = (int)std::floor( 10 * image.readtime / image.linecount );  // line time in 10 msec
      linetime *= 90;                                                           // line time in usec (here's the 90%)

      // initialize random seed for data
      //
      std::srand( time( NULL ) );

      try {
        frame.bufpixels.at( frame.index ) = 0;
        frame.buflines.at( frame.index ) = 0;
        frame.bufcomplete.at( frame.index ) = 0;

        int i=0;

        std::cerr << function << "readout line: ";
        for ( frame.buflines.at(frame.index) = 0; frame.buflines.at(frame.index) < image.linecount; frame.buflines.at(frame.index)++ ) {
          for ( frame.bufpixels.at(frame.index)= 0; frame.bufpixels.at(frame.index) < image.pixelcount; frame.bufpixels.at(frame.index)++ ) {
            for ( int tap = 0; tap < image.taplines; tap++ ) {
//            frame.buffer.at( i ) = rand() % 40000 + 30000;  // random number between {30k:40k}
              i++;
            }
//          frame.bufpixels.at( frame.index )++;
          }
//        frame.buflines.at( frame.index )++;
          std::cerr << std::dec << std::setw(6) << frame.buflines.at(frame.index) << "\b\b\b\b\b\b";
          usleep( linetime );
        }
        std::cerr << std::dec << std::setw(6) << frame.buflines.at(frame.index) << " complete\n";
        frame.bufcomplete.at( frame.index ) = 1;
        image.framen++;
        frame.bufframen.at( frame.index ) = image.framen;
      }
      catch( std::out_of_range & ) {
        std::cerr << function << "ERROR: frame.index=" << frame.index << " out of range\n";
        return;
      }
      catch( ... ) {
        std::cerr << function << "unknown error using frame index " << frame.index << "\n";
        return;
      }
    }

  }
  /**************** Interface::dothread_expose ********************************/
}
