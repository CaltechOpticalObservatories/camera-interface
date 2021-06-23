#include "emulator-archon.h"
namespace Archon {

  // Archon::Interface constructor
  //
  Interface::Interface() {
    this->modtype.resize( nmods );
    this->modversion.resize( nmods );
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

    }

    return( NO_ERROR );
  }
  /**************** Interface::configure_controller ***************************/


  /**************** Interface:wrconfig ****************************************/
  /**
   * @fn     wconfig
   * @brief  handles the incoming WCONFIG command
   * @param  buf, incoming command string
   * @return ERROR or NO_ERROR
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

std::cerr << function << "stored configmap[ " << linenumber 
          << " ].key=" << this->configmap[ linenumber ].key
          << " .value=" << this->configmap[ linenumber ].value
          << "\n";

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
      } // end else
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

//  std::cerr << "(Archon::Interface::wconfig) " << buf << "\n";
    return( NO_ERROR );
  }
  /**************** Interface:wrconfig ****************************************/


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
std::cerr << function << "retsring=" << retstring << "\n";
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


  /**************** Interface::system *****************************************/
  /**
   * @fn     system
   * @brief  handles the incoming SYSTEM command
   * @param  buf, incoming command string
   * @param  &retstring, reference to string for return values
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::system(std::string buf, std::string &retstring) {
    std::string function = "(Archon::Interface::system) ";
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
  /**************** Interface::system *****************************************/


  /**************** Interface::write_parameter ********************************/
  /**
   * @fn     write_parameter
   * @brief  
   * @param  
   * @param  
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::write_parameter(std::string buf) {
    std::string function = "(Archon::Interface::write_parameter) ";
    std::vector<std::string> tokens;
    std::string key="";
    std::string value="";
    std::string line="";

    Tokenize( buf, tokens, " " );

    if ( tokens.size() != 2 ) {
      std::cerr << function << "ERROR: expected <Paramname> <value> but received " << buf << "\n";
      return( ERROR );
    }

    try {
      key =   tokens.at(0);
      value = tokens.at(1);
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

    if ( this->parammap.find( key ) == this->parammap.end() ) {
      std::cerr << function << "ERROR: " << key << " not found in parammap\n";
      return( ERROR );
    }

    else {
      line = this->parammap[ key ].line;
      this->configmap[ line ].value = value;
      this->parammap[ key ].value = value;       //TODO needed??
    }

    return( NO_ERROR );
  }
  /**************** Interface::write_parameter ********************************/

}
