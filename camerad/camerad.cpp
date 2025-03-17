
#include "camerad.h"

int main( int argc, char** argv ) {
  Daemon::daemonize( "cleand", "/tmp", "/dev/null", "/tmp/cleand.stderr", "", false );
  std::cerr << "daemonized. child process running\n";

  Camera::Server camerad;

  std::this_thread::sleep_for( std::chrono::milliseconds(500) );

  camerad.get_interface().init_pubsub();

  std::this_thread::sleep_for( std::chrono::milliseconds(500) );

  camerad.get_interface().publish_snapshot();

  Network::TcpSocket sock_block(8675, true, -1, 0);
  if ( sock_block.Listen() < 0 ) {
    std::cerr << "ERROR could not create listening socket\n";
    exit(1);
  }

  while ( true ) {
    auto newid = camerad.id_pool.get_next_number();
    {
    std::lock_guard<std::mutex> lock(camerad.sock_block_mutex);
    camerad.socklist[newid] = std::make_shared<Network::TcpSocket>(sock_block);  // create a new socket
    camerad.socklist[newid]->id = newid;        // update the id of the copied socket
    camerad.socklist[newid]->Accept();          // accept connections on the new socket
    }

    // Create a new thread to handle the connection
    //
    std::thread( &Camera::Server::block_main, &camerad, camerad.socklist[newid] ).detach();
  }

  for (;;) pause();                                  // main thread suspends

  return 0;
}

