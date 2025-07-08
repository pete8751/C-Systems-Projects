#include "router.hh"
#include "arp_message.hh"
#include "network_interface_test_harness.hh"
#include "router_common_2.hh"

#include <iostream>
#include <list>
#include <random>
#include <unordered_map>
#include <utility>

using namespace std;

void network_simulator()
{
  const string green = "\033[32;1m";
  const string normal = "\033[m";

  cerr << green << "Constructing network." << normal << "\n";

  Network network;

  cout << green << "\n\n Routing a large number of Packets ..." << normal
       << "\n\n";

  {
    for (int i = 0; i < 3; i++) {
        auto dgram_sent = network.host( "cherrypie" ).send_to( network.host( "doughnut" ).address() );
        dgram_sent.header.ttl--;
        dgram_sent.header.compute_checksum();
        network.host( "doughnut" ).expect( dgram_sent );

        dgram_sent = network.host( "applesauce" ).send_to( network.host( "blueberrymuffin" ).address() );
        dgram_sent.header.ttl--;
        dgram_sent.header.compute_checksum();
        network.host( "blueberrymuffin" ).expect( dgram_sent );

        dgram_sent = network.host( "cherrypie" ).send_to( network.host( "blueberrymuffin" ).address() );
        dgram_sent.header.ttl--;
        dgram_sent.header.compute_checksum();
        network.host( "blueberrymuffin" ).expect( dgram_sent );

        dgram_sent = network.host( "doughnut" ).send_to( network.host( "blueberrymuffin" ).address() );
        dgram_sent.header.ttl--;
        dgram_sent.header.compute_checksum();
        network.host( "blueberrymuffin" ).expect( dgram_sent );
    }

    network.simulate();
  }

  cout << "\n\n\033[32;1mCongratulations! All datagrams were routed successfully.\033[m\n";
}

int main()
{
  try {
    network_simulator();
  } catch ( const exception& e ) {
    cerr << "\n\n\n";
    cerr << "\033[31;1mError: " << e.what() << "\033[m\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
