#pragma once

namespace Archon {

  /***** Archon::DeInterlaceBase **********************************************/
  /**
   * @class      Archon::DeInterlaceBase
   * @brief      deinterlacing base class
   *
   */
  class DeInterlaceBase {
    public:
      virtual ~DeInterlaceBase() = default;
      virtual void deinterlace() = 0;
  };
  /***** Archon::DeInterlaceBase **********************************************/


  /***** Archon::DeInterlace **************************************************/
  /**
   * @class      Archon::DeInterlace
   * @brief      template class
   *
   */
  template <typename T>
  class DeInterlace : public DeInterlaceBase {
    protected:
      std::vector<T> input_buffer;
      size_t imgsz;

    public:
      DeInterlace(std::vector<T> bufin, size_t imgszin) : input_buffer(std::move(bufin)), imgsz(imgszin) { }

      virtual void do_deinterlace( std::vector<T> &buffer ) = 0;

      void deinterlace() override {
        do_deinterlace(input_buffer);
      }
  };
  /***** Archon::DeInterlace **************************************************/


  /***** Archon::DeInterlace_RXRVideo *****************************************/
  /**
   * @class      Archon::DeInterlace_RXRVideo
   * @brief      derived class for deinterlace mode RXRVIDEO
   *
   */
  template <typename T>
  class DeInterlace_RXRVideo : public DeInterlace<T> {
    private:
      std::vector<std::vector<T>> _sigbuf;
      std::vector<std::vector<T>> _resbuf;

    public:
      DeInterlace_RXRVideo( const std::vector<T> &bufin, const size_t imgsz )
        : DeInterlace<T>(bufin,imgsz),
          _sigbuf(2, std::vector<T>(imgsz)),
          _resbuf(2, std::vector<T>(imgsz)) { }

      DeInterlace_RXRVideo( const char* bufin, size_t bufsz, int hdrshift=0 )
        : DeInterlace<T>(bufin, bufsz,hdrshift),
          _sigbuf(2, std::vector<T>(bufsz)),
          _resbuf(2, std::vector<T>(bufsz)) { }

      /**
       * @brief      returns one of the requested _sigbufs by index
       * @param[in]  idx  index of which buffer to return
       * @return     reference to _sigbuf[idx]
       */
      const std::vector<T> &get_sigbuf(size_t idx) const {
        if ( idx > _sigbuf.size() ) {
          std::stringstream message;
          message << "DeInterlace_RXRVideo::get_sigbuf index " << idx << " out of range: " << _sigbuf.size();
          throw std::out_of_range( message.str() );
        }
        else return _sigbuf[idx];
      }

      /**
       * @brief      returns one of the requested _resbufs by index
       * @param[in]  idx  index of which buffer to return
       * @return     reference to _resbuf[idx]
       */
      const std::vector<T> &get_resbuf(size_t idx) const {
        if ( idx > _resbuf.size() ) {
          std::stringstream message;
          message << "DeInterlace_RXRVideo::get_resbuf index " << idx << " out of range: " << _resbuf.size();
          throw std::out_of_range( message.str() );
        }
        else return _resbuf[idx];
      }

      // implement specific logic for mode "RXRVIDEO"
      void do_deinterlace( std::vector<T> &buffer ) override {
      }
  };
  /***** Archon::DeInterlace_RXRVideo *****************************************/


  /***** Archon::DeInterlace_None *********************************************/
  /**
   * @class      Archon::DeInterlace_None
   * @brief      derived class for deinterlace mode NONE
   * @details    This class inherits from the DeInterlace template class,
   *             which inherits from the DeInterlaceBase class.
   *
   */
  template <typename T>
  class DeInterlace_None : public DeInterlace<T> {
    public:
      // inherit constructor
      using DeInterlace<T>::DeInterlace;

      // implement specific logic for mode "NONE"
      void do_deinterlace( std::vector<T> &buffer ) override {
      }
  };
  /***** Archon::DeInterlace_None *********************************************/


  /***
  template <typename T>
  std::unique_ptr<DeInterlaceBase> createDeInterlacer( const std::vector<T> &buffer,
                                                       const std::string &mode ) {
    if ( mode == "none" ) {
      return std::make_unique<DeInterlace_None<T>>(buffer);
    }
    return nullptr;
  }
  ***/

  template <typename T>
  std::unique_ptr<DeInterlaceBase> deinterlace_factory( const std::string &mode,
                                                        const std::vector<T> buf,
                                                        const size_t imgsz ) {
    std::stringstream message;
    if ( mode == "none" ) {
      message << "[DEBUG] created deinterlacer for datatype " << demangle(typeid(T).name());
      logwrite( "deinterlacer_factory", message.str() );
      return std::make_unique<DeInterlace_None<T>>(buf, imgsz);
    }
    else
    if ( mode == "rxrv" ) {
      message << "[DEBUG] created deinterlacer for datatype " << demangle(typeid(T).name());
      logwrite( "deinterlacer_factory", message.str() );
      return std::make_unique<DeInterlace_RXRVideo<T>>(buf, imgsz);
    }
    else {
      logwrite( "deinterlacer_factory", "ERROR unknown mode: "+mode );
      return nullptr;
    }
  }


}
