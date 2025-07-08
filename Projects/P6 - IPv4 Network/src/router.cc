#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  //prefix mask will only have nonzero values in first prefix_length bits.
  uint32_t prefix_mask = get_prefmask(prefix_length, route_prefix); //This is the mask for the prefix. If the prefix has bits 1001...., and prefix length is 4, the mask would be 0000...1001

  //Add all this data to the given table, at the given prefix.
  routing_table_[prefix_length][prefix_mask].first = interface_num;

  if (next_hop != nullopt){
    routing_table_[prefix_length][prefix_mask].second = (next_hop.value()).ipv4_numeric(); 
  } else {
    routing_table_[prefix_length][prefix_mask].second = {};
  }

}

uint32_t Router::get_prefmask(uint8_t prefix_length, const uint32_t route){

  //I assume that there is always a default available if no matches happen.
  if (prefix_length == 0){ //special case, no matching bits required.
    return 0;
  }
  
  return (route >> (32 - prefix_length));
}

optional<pair<size_t, uint32_t>> Router::find_match(uint32_t dst_ip){
  //In this function, we want to check if there is a corresponding entry here.
  
  uint32_t ip_mask;
  pair<size_t, uint32_t> res;
  for (int prefix_length = 32; prefix_length >= 0; prefix_length--){

    ip_mask = get_prefmask(prefix_length, dst_ip);

    //check if there is a match at this prefix length in the table:
    if (routing_table_[prefix_length].find(ip_mask) != routing_table_[prefix_length].end()){
      res.first = routing_table_[prefix_length][ip_mask].first; //we've found a match (this is the longest prefix match, as we iterate through prefix lengths backward)

      if (routing_table_[prefix_length][ip_mask].second == nullopt){ //No next hop was filled in the table, so we know that the next hop is simply the destination IP.
        res.second = dst_ip;
      } else {
        res.second = routing_table_[prefix_length][ip_mask].second.value();
      }

      return res; //we return our result.
    }
  } 
  
  return {}; //we couldn't find any matches, so we return default. 
}

void Router::process_dgram(InternetDatagram& dgram){

  //first check if TTL has been reached. 
  if (dgram.header.ttl <= 1){
    return; //drop packet. 
  } else {
    dgram.header.ttl--;
    dgram.header.compute_checksum();
  }

  optional<pair<size_t, uint32_t>> routing_info; 
  routing_info = find_match(dgram.header.dst);

  if (routing_info != nullopt){ //We found a location, so we send the packet now.
    AsyncNetworkInterface& outgoing_intf = interface(routing_info.value().first); //Get the outgoing interface we need to send the datagram on.
    outgoing_intf.send_datagram(dgram, Address::from_ipv4_numeric(routing_info.value().second)); //send datagram, with next hop data.
  }

  return; //if we don't find a location, we simply return without doing anything (packet dropped)
}

void Router::process_interface(size_t interface_num){
  //We assume the interface_num is valid and in bounds here. 

    //Get the interface that we want to process. 
  AsyncNetworkInterface& targ_intf = interface(interface_num);
  optional<InternetDatagram> dgram = targ_intf.maybe_receive(); //get a packet from the queue. 

  while (dgram.has_value()){//Keep iterating through queue until it is empty. 
    process_dgram(dgram.value());
    dgram = targ_intf.maybe_receive();
  }

  return;
}

void Router::route() {

  //Check through all interfaces for packets that need processing:
  for (size_t i = 0; i < (interfaces_).size(); i++) {
    process_interface(i); //Processes all packets in the queue.
  }
}
