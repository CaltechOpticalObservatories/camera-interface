#include "camera.h"

namespace Camera {

  /***** Camera::Camera::log_error ********************************************/
  /**
   * @brief      logs the error and saves the message to be returned on the command port
   * @param[in]  function  string containing the name of the Namespace::Class::function
   * @param[in]  message   string containing error message
   * @return     ERROR or NO_ERROR
   *
   */
  void Camera::log_error( std::string function, std::string message ) {
    std::stringstream err;

    // Save this message in class variable
    this->lasterrorstring.str("");
    this->lasterrorstring << message;

    // Form an error string as "ERROR: <message>"
    err << "ERROR: " << this->lasterrorstring.str();

    // Log and send to async port in the usual ways
    //
    logwrite( function, err.str() );
//  this->async.enqueue( err.str() );
  }
  /***** Camera::Camera::log_error ********************************************/

}
