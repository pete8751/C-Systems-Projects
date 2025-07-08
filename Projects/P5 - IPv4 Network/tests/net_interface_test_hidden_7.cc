#include "arp_message.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "network_interface_test_harness.hh"

#include <cstdlib>
#include <iostream>
#include <random>

using namespace std;

EthernetAddress random_private_ethernet_address()
{
  EthernetAddress addr;
  for ( auto& byte : addr ) {
    byte = random_device()(); // use a random local Ethernet address
  }
  addr.at( 0 ) |= 0x02; // "10" in last two binary digits marks a private Ethernet address
  addr.at( 0 ) &= 0xfe;

  return addr;
}

InternetDatagram make_datagram( const string& src_ip, const string& dst_ip ) // NOLINT(*-swappable-*)
{
  InternetDatagram dgram;
  dgram.header.src = Address( src_ip, 0 ).ipv4_numeric();
  dgram.header.dst = Address( dst_ip, 0 ).ipv4_numeric();
  dgram.payload.emplace_back( "hello" );
  dgram.header.len = static_cast<uint64_t>( dgram.header.hlen ) * 4 + dgram.payload.size();
  dgram.header.compute_checksum();
  return dgram;
}

ARPMessage make_arp( const uint16_t opcode,
                     const EthernetAddress sender_ethernet_address,
                     const string& sender_ip_address,
                     const EthernetAddress target_ethernet_address,
                     const string& target_ip_address )
{
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = sender_ethernet_address;
  arp.sender_ip_address = Address( sender_ip_address, 0 ).ipv4_numeric();
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = Address( target_ip_address, 0 ).ipv4_numeric();
  return arp;
}

EthernetFrame make_frame( const EthernetAddress& src,
                          const EthernetAddress& dst,
                          const uint16_t type,
                          vector<Buffer> payload )
{
  EthernetFrame frame;
  frame.header.src = src;
  frame.header.dst = dst;
  frame.header.type = type;
  frame.payload = std::move( payload );
  return frame;
}

int main()
{
  try {
    const EthernetAddress local_eth = random_private_ethernet_address();
    NetworkInterfaceTestHarness test{"Edge Case Tests", local_eth, Address("10.1.1.1", 0)};

    const EthernetAddress remote_eth = random_private_ethernet_address();
    const EthernetAddress wrong_eth = random_private_ethernet_address();

    // Step 1: Send datagram, expect ARP request
    const auto datagram = make_datagram("10.1.1.1", "10.1.1.2");
    test.execute(SendDatagram{datagram, Address("10.1.1.2", 0)});
    test.execute(ExpectFrame{make_frame(local_eth, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP,
                                         serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth,
                                                            "10.1.1.1", {}, "10.1.1.2"))) });
    
    // Step 2: An ARP reply that is not ours arrives, it should be ignored
    test.execute(ReceiveFrame{make_frame(remote_eth, wrong_eth,
                                         EthernetHeader::TYPE_ARP,
                                         serialize(make_arp(ARPMessage::OPCODE_REPLY,
                                                            remote_eth, "10.1.1.2",
                                                            wrong_eth, "10.1.1.1"))), {} });
    test.execute(ExpectNoFrame{}); // Nothing should happen
    
    // Step 3: Send another datagram, nothing should happen and datagram should be queued up
    test.execute(SendDatagram{datagram, Address("10.1.1.2", 0)});
    test.execute(ExpectNoFrame{});

    // Step 4: Malformed frame arrives, should be ignored
    vector<Buffer> corrupted_payload = {Buffer("malformed_data")};
    test.execute(ReceiveFrame{make_frame(remote_eth, ETHERNET_BROADCAST,
                                         0x345, // garbage number
                                         corrupted_payload), {}});
                            
    test.execute(ReceiveFrame{make_frame(remote_eth, ETHERNET_BROADCAST,
                                         EthernetHeader::TYPE_ARP,
                                         corrupted_payload), {}});

    test.execute(ExpectNoFrame{});

    // Step 5: The correct ARP reply finally arrives, the datagram should be sent
    test.execute(ReceiveFrame{make_frame(remote_eth, local_eth,
                                         EthernetHeader::TYPE_ARP,
                                         serialize(make_arp(ARPMessage::OPCODE_REPLY,
                                          remote_eth, "10.1.1.2",
                                          local_eth, "10.1.1.1"))), {} });
    
    test.execute(ExpectFrame{make_frame(local_eth, remote_eth, EthernetHeader::TYPE_IPv4,
                                         serialize(datagram))});
    test.execute(ExpectFrame{make_frame(local_eth, remote_eth, EthernetHeader::TYPE_IPv4,
                                          serialize(datagram))});
    test.execute(ExpectNoFrame{});
                               

  } catch ( const exception& e ) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
