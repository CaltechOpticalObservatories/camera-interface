/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ARCHON_H
#define ARCHON_H

namespace Archon {

  class Interface {
    private:
    public:
      Interface();
      ~Interface();

      long connect_to_controller();
      long close_connection();
      long load_config(std::string cfgfile);
  };

}

#endif
