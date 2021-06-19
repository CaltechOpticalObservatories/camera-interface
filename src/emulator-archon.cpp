#include "emulator-archon.h"
namespace Archon {

  Interface::Interface() {
  }
  Interface::~Interface() {
  }

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

        // build an STL map "parammap" indexed on linenumber because that's what Archon uses
        //
        this->parammap[ linenumber ].key   = tokens.at(0); // PARAMETERn
        this->parammap[ linenumber ].name  = tokens.at(1); // ParameterName
        this->parammap[ linenumber ].value = tokens.at(2); // value
        this->parammap[ linenumber ].line  = linenumber;   // line number
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


  long Interface::rconfig(std::string buf, std::string &retstring) {
    std::string function = "(Archon::Interface::rconfig) ";
    std::string linenumber;

    if ( buf.length() != 11 ) {                       // check for minimum length "RCONFIGxxxx", 11 chars
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
}
