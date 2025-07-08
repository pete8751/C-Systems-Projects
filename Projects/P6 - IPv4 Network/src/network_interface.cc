#include "network_interface.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

//TODO: DO WE NEED TO PUT THE INTERFACES OWN IP -> MAC ADDRESS MAPPING IN THE ARP TABLE?


// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address, 
                                   const Address& ip_address) : 
    ethernet_address_(ethernet_address), 
    ip_address_(ip_address),
    arp_table_({}),
    arp_reqs_({}),
    rtosend_q_() {

    cerr << "DEBUG: Network interface has Ethernet address ";
    cerr << to_string(ethernet_address_);
    cerr << " and IP address ";
    cerr << ip_address.ip() << "\n";
}

//The following two functions were copied from the test files.  (modified make_arp)
EthernetFrame NetworkInterface::construct_frame( const EthernetAddress& src,
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

ARPMessage NetworkInterface::construct_arp( const uint16_t opcode,
                     const EthernetAddress& sender_ethernet_address,
                     const Address& sender_ip_address,
                     const EthernetAddress& target_ethernet_address,
                     const Address& target_ip_address )
{
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = sender_ethernet_address; 
  arp.sender_ip_address = sender_ip_address.ipv4_numeric();
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = target_ip_address.ipv4_numeric();
  return arp;
}

void NetworkInterface::queue_ip_packet(const InternetDatagram& dgram, const EthernetAddress& target_mac_addr){
    EthernetFrame new_frame = construct_frame(ethernet_address_, target_mac_addr, EthernetHeader::TYPE_IPv4, serialize(dgram)); //create the eth frame.
    rtosend_q_.push(new_frame);
}

void NetworkInterface::queue_arp_req(const Address& target_addr) {
    //first we check if there is a prexisting queue of packets that have requested this MAC address within the last 5 seconds. 
    if (arp_reqs_.find(target_addr.ipv4_numeric()) != arp_reqs_.end()){
        //we have already requested for the MAC address related to this IP within the last 5 seconds.
        return;
    }

    //We must create a new request for this MAC address.
    ARPMessage arp_req = construct_arp(ARPMessage::OPCODE_REQUEST, ethernet_address_, ip_address_, {}, target_addr); //create the request.
    EthernetFrame new_frame = construct_frame(ethernet_address_, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, serialize(arp_req)); //create the eth frame.
    //this eth frame will be broadcasted.
    rtosend_q_.push(new_frame); //push it onto the send queue. 
}

void NetworkInterface::queue_arp_reply(const Address& target_addr, const EthernetAddress& target_mac_addr) {
    //We must create a reply to the target mac address.
    ARPMessage arp_reply = construct_arp(ARPMessage::OPCODE_REPLY, ethernet_address_, ip_address_, target_mac_addr, target_addr); //create the reply.
    EthernetFrame new_frame = construct_frame(ethernet_address_, target_mac_addr, EthernetHeader::TYPE_ARP, serialize(arp_reply)); //create the eth frame.

    rtosend_q_.push(new_frame); //push it onto the send queue. 
}


// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram, 
                                     const Address& next_hop){
    EthernetAddress mac_addr;

    //First we check if we know the MAC address of the next hop:
    if (arp_table_.find(next_hop.ipv4_numeric()) != arp_table_.end()){
        mac_addr = arp_table_[next_hop.ipv4_numeric()].first; //we have the MAC_addr.
        queue_ip_packet(dgram, mac_addr); //create the ethernet frame for the packet, and queue it to be sent.

    } else {
        queue_arp_req(next_hop); //sends a request for the destination mac_address. (or not if we already requested it recently)
        arp_reqs_[next_hop.ipv4_numeric()].first.push(dgram); //add this dgram to the queue waiting for the ARP response.
    }
}

optional<InternetDatagram> NetworkInterface::recieve_ipdgram(const EthernetFrame& frame){
    //attempt to parse, returning null on error. 
    InternetDatagram incoming_dgram = {};
    if (parse(incoming_dgram, (&frame)->payload)){
        return incoming_dgram;
    }

    return {};
}

void NetworkInterface::release_reqs_q(uint32_t target_addr_bin, EthernetAddress target_mac_addr){
    //first we check that there is a queue waiting on this 
    if (arp_reqs_.find(target_addr_bin) == arp_reqs_.end()){return;} //No/empty queue, so nothing to be done.

    //Queue all packets that were waiting for this MAC address.
    while (!(arp_reqs_[target_addr_bin].first).empty()){
        queue_ip_packet((arp_reqs_[target_addr_bin].first).front(), target_mac_addr);

        (arp_reqs_[target_addr_bin].first).pop();
    }

    arp_reqs_.erase(target_addr_bin); //remove this entry in the map.
}

void NetworkInterface::recieve_arp(const EthernetFrame& frame){
    ARPMessage arp_reply = {};

    //update the table, and queue all packets that were waiting for this reply.
    if (parse(arp_reply, frame.payload)){ //The payload is an ARP Reply
        arp_table_[arp_reply.sender_ip_address].first = arp_reply.sender_ethernet_address; //add or replace an entry in the table. 
        arp_table_[arp_reply.sender_ip_address].second = 0;

        release_reqs_q(arp_reply.sender_ip_address, arp_reply.sender_ethernet_address); //Queue all packets that were waiting for this MAC address to be sent.
    }

}

void NetworkInterface::reply_arp(const EthernetFrame& frame){
    ARPMessage arp_req = {};

    //update the table, and respond to the request if needed
    if (parse(arp_req, frame.payload)){ //The payload is an ARP Req
        arp_table_[arp_req.sender_ip_address].first = arp_req.sender_ethernet_address; //add or replace an entry in the table. 
        arp_table_[arp_req.sender_ip_address].second = 0;

        release_reqs_q(arp_req.sender_ip_address, arp_req.sender_ethernet_address); //Queue all packets that were waiting for this MAC address to be sent.
        
        //Now create a reply for this ARP request, in the case that the ARP requested for this interface's mac:
        if (arp_req.target_ip_address == ip_address_.ipv4_numeric()){
            queue_arp_reply(Address::from_ipv4_numeric(arp_req.sender_ip_address), arp_req.sender_ethernet_address);
        }
    }

}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame& frame) {

    //First we check if the frame was destined for this interface specifically.
    if (frame.header.dst == ethernet_address_){

        if (frame.header.type == EthernetHeader::TYPE_IPv4){ //This is an ipv4 datagram destined for the interface

            return recieve_ipdgram(frame);

        } else { //handle the frame if it is an ARP reply.
            recieve_arp(frame);
        }
    }

    //Next we check if the frame was broadcast (possibly ARP)
    if (frame.header.dst == ETHERNET_BROADCAST){
        //Handle the frame if it is an ARP request.
        reply_arp(frame);
    }


    return {}; 
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick){

    //iterate through all entries in the arp_table, incrementing the timers, and removing expired entries:
    for (auto it = arp_table_.begin(); it != arp_table_.end(); ) {
        auto& key_val = it->second;
        key_val.second += ms_since_last_tick; // Add time since last tick
    
        if (key_val.second >= 30000) { // check for 30-second limit
            it = arp_table_.erase(it);  
        } else {
            ++it; 
        }
    }

    //Doing the same for packets waiting for ARP reply:
    for (auto it = arp_reqs_.begin(); it != arp_reqs_.end(); ) {
        auto& key_val = it->second;
        key_val.second += ms_since_last_tick; // Add time since last tick
    
        if (key_val.second >= 5000) { // check for 5-second limit
            it = arp_reqs_.erase(it);  
        } else {
            ++it; 
        }
    }

    //Can't combine these without using templates, as the maps are of different types. 
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{   
    //Check for a frame on the queue. If there is one, pop it off, and send it. 
    if (!rtosend_q_.empty()){
        EthernetFrame nextframe = rtosend_q_.front();
        rtosend_q_.pop();
        return nextframe;
    }

    return {}; 
}
