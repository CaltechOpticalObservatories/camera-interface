/** ---------------------------------------------------------------------------
 * @file     server.h
 * @brief    
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     
 * @modified 
 *
 */

#ifndef EMULATOR_SERVER_H
#define EMULATOR_SERVER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <csignal>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#ifdef ASTROCAM
#include "emulator-astrocam.h"    //!< for any Leach interface, ARC-64 or ARC-66
#elif STA_ARCHON
#include "emulator-archon.h"      //!< for STA-Archon
#endif

#include "logentry.h"
#include "config.h"
#include "network.h"

#define  BUFSIZE      1024  //!< size of the input command buffer
#define  CONN_TIMEOUT 3000  //<! incoming (non-blocking) connection timeout in milliseconds

namespace Emulator {
#ifdef ASTROCAM
  class Server : public AstroCam::Interface {
#elif STA_ARCHON
    class Server : public Archon::Interface {
#endif

    private:
    public:
        Server() {
            this->emulatorport = -1;
            this->nbport = -1;
            this->blkport = -1;
            this->asyncport = -1;
        }

        /** Emulator::~Server ********************************************************/
        /**
         * @fn     ~Server
         * @brief  class deconstructor cleans up on exit
         */
        ~Server() {
            close(this->blocking_socket);
        }

        /** Emulator::~Server ********************************************************/

        int emulatorport; //!< emulator port
        int nbport; //!< non-blocking port
        int blkport; //!< blocking port
        int asyncport; //!< asynchronous message port
        std::string asyncgroup; //!< asynchronous multicast group

        int nonblocking_socket;
        int blocking_socket;

        Network::TcpSocket nonblocking;

        std::mutex conn_mutex; //!< mutex to protect against simultaneous access to Accept()

        /** Emulator::Server::exit_cleanly *******************************************/
        /**
         * @fn     signal_handler
         * @brief  handles ctrl-C and exits
         * @param  int signo
         * @return nothing
         *
         */
        void exit_cleanly(void) {
            std::cerr << "server exiting\n";
            exit(EXIT_SUCCESS);
        }

        /** Emulator::Server::exit_cleanly *******************************************/


        /** Emulator::Server::configure_server ***************************************/
        /**
         * @fn     configure_server
         * @brief
         * @param  none
         * @return ERROR or NO_ERROR
         *
         */
        long configure_server() {
            std::string function = "(Emulator::Server::configure_server) ";
            std::stringstream message;
            int applied = 0;
            long error;

            // loop through the entries in the configuration file, stored in config class
            //
            for (int entry = 0; entry < this->config.n_entries; entry++) {
                // EMULATOR_PORT
                if (config.param[entry].compare(0, 13, "EMULATOR_PORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        std::cerr << function << "ERROR: bad EMULATOR_PORT: unable to convert to integer\n";
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        std::cerr << function << "EMULATOR_PORT number out of integer range\n";
                        return (ERROR);
                    }
                    this->emulatorport = port;
                    applied++;
                }

                // NBPORT
                if (config.param[entry].compare(0, 6, "NBPORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        std::cerr << function << "ERROR: bad NBPORT: unable to convert to integer\n";
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        std::cerr << function << "NBPORT number out of integer range\n";
                        return (ERROR);
                    }
                    this->nbport = port;
                    applied++;
                }

                // BLKPORT
                if (config.param[entry].compare(0, 7, "BLKPORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        std::cerr << function << "ERROR: bad BLKPORT: unable to convert to integer\n";
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        std::cerr << function << "BLKPORT number out of integer range\n";
                        return (ERROR);
                    }
                    this->blkport = port;
                    applied++;
                }

                // ASYNCPORT
                if (config.param[entry].compare(0, 9, "ASYNCPORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        std::cerr << function << "ERROR: bad ASYNCPORT: unable to convert to integer\n";
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        std::cerr << function << "ASYNCPORT number out of integer range\n";
                        return (ERROR);
                    }
                    this->asyncport = port;
                    applied++;
                }

                // ASYNCGROUP
                if (config.param[entry].compare(0, 10, "ASYNCGROUP") == 0) {
                    this->asyncgroup = config.arg[entry];
                    applied++;
                }
            } // end loop through the entries in the configuration file

            message.str("");
            if (applied == 0) {
                message << "ERROR: ";
                error = ERROR;
            } else {
                error = NO_ERROR;
            }
            message << "applied " << applied << " configuration lines to server";
            std::cerr << function << message.str() << "\n";
            return error;
        }

        /** Emulator::Server::configure_server ***************************************/
    }; // end class Server
} // end namespace Emulator
#endif
