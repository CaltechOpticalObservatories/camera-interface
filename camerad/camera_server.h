#pragma once

#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <limits.h>

#ifdef ASTROCAM
  #include "astrocam_interface.h"
#elif STA_ARCHON
  #include "archon.h"
#endif

#include "utilities.h"
#include "network.h"
#include "camerad_commands.h"
#include "number_pool.h"

namespace Camera {

  class Server {
    private:
      #ifdef ASTROCAM
        AstroCam::Interface interface;
      #elif STA_ARCHON
        Archon::Interface interface;
      #endif

    public:
      Server() :
       id_pool(10),
       threads_active(0) {
       }

      ~Server() = default;

      #ifdef ASTROCAM
        AstroCam::Interface &get_interface();
        std::map<int, AstroCam::Controller> &get_controllers();
      #elif STA_ARCHON
        Archon::Interface &get_interface();
      #endif

      NumberPool id_pool;
      std::map<int, std::shared_ptr<Network::TcpSocket>> socklist;
      std::mutex sock_block_mutex;
      std::atomic<int> threads_active;
      std::atomic<int> cmd_num;

      void block_main(std::shared_ptr<Network::TcpSocket> socket);

      void doit(Network::TcpSocket sock);
  };
}

