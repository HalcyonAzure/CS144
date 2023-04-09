#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"

#include <cstdint>
#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    optional<EthernetAddress> next_eth;

    // 检查next_hop的IP地址是否在ARP里面有
    for (const auto &entry : _arp_table) {
        if (entry.first.raw_ip_addr == next_hop_ip) {
            next_eth = entry.first.eth_addr;
            break;
        }
    }
    // 如果在ARP里面有则直接发送并短路
    if (next_eth.has_value()) {
        EthernetFrame eth_frame;
        eth_frame.header() = {next_eth.value(), _ethernet_address, EthernetHeader::TYPE_IPv4};
        eth_frame.payload() = dgram.serialize();
        _frames_out.push(eth_frame);
        return;
    }

    // ARP内没有，先判断之前是否已经发送过探针，如果发送过就不发送了
    for (auto &probe : _probe_table) {
        if (probe.first.raw_ip_addr == next_hop_ip) {
            return;
        }
    }

    // 如果没发送就发送，并且将这个探针加入探针表
    ARPMessage arp_probe;
    arp_probe.opcode = ARPMessage::OPCODE_REQUEST;
    arp_probe.sender_ethernet_address = _ethernet_address;
    arp_probe.sender_ip_address = _ip_address.ipv4_numeric();
    arp_probe.target_ethernet_address = {};
    arp_probe.target_ip_address = next_hop_ip;
    EthernetFrame probe_frame;
    probe_frame.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
    probe_frame.payload() = arp_probe.serialize();
    _frames_out.push(probe_frame);

    // 加入缓存表
    ArpProbe _arp = {next_hop_ip, dgram};
    _probe_table[_arp] = arp_probe_ttl;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 丢弃目标MAC地址不是我自己的数据帧
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return {};
    }

    // 接受到IP数据段的时候（代表对方和自己都有了互相的ARP信息，不需要对ARP表进行操作），对这个数据段进行处理
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        datagram.parse(frame.payload());
        return datagram;
    }

    // 接受到的是一个ARP包，先将这个包的内容序列化，并将其中包含的ARP信息尝试更新到自己的ARP表中
    ARPMessage arp_msg;
    arp_msg.parse(frame.payload());
    ArpEntry src = {arp_msg.sender_ip_address, arp_msg.sender_ethernet_address},
             dst = {arp_msg.target_ip_address, arp_msg.target_ethernet_address};

    _update_arp_table({src, dst});

    // 过滤掉不是发给自己的IP地址的包
    // if (dst.raw_ip_addr != _ip_address.ipv4_numeric()) {
    //     return {};
    // }

    // 接受到ARP请求的时候，返回一个包含自己信息的ARP响应报文，同时利用这个frame更新自己的ARP表
    if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST) {
        ARPMessage reply;
        reply.opcode = ARPMessage::OPCODE_REPLY;
        reply.sender_ip_address = _ip_address.ipv4_numeric();
        reply.sender_ethernet_address = _ethernet_address;
        reply.target_ip_address = src.raw_ip_addr;
        reply.target_ethernet_address = src.eth_addr;

        EthernetFrame reply_frame;
        reply_frame.header() = {src.eth_addr, _ethernet_address, EthernetHeader::TYPE_ARP};
        reply_frame.payload() = reply.serialize();
        _frames_out.push(reply_frame);
        return {};
    }

    // 收到别人传送回来的ARP的时候，如果缓存中有等待的对应条目，则删除，并发送对应的数据
    for (auto entry = _probe_table.begin(); entry != _probe_table.end(); entry++) {
        if (entry->first.raw_ip_addr == src.raw_ip_addr) {
            send_datagram(entry->first.datagram, Address::from_ipv4_numeric(entry->first.raw_ip_addr));
            auto probe = entry->first;
            entry++;
            _probe_table.erase(probe);
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 将所有超过TTL的ARP条目都删除
    for (auto entry = _arp_table.begin(); entry != _arp_table.end(); entry++) {
        if (entry->second < ms_since_last_tick) {
            // _arp_table.erase(entry->first);
            auto arp = entry->first;
            _arp_table.erase(arp);
            entry++;
        } else {
            entry->second -= ms_since_last_tick;
        }
    }

    for (auto entry = _probe_table.begin(); entry != _probe_table.end(); entry++) {
        if (entry->second < ms_since_last_tick) {
            EthernetFrame re_probe;
            re_probe.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
            re_probe.payload() = re_probe.serialize();
            _frames_out.push(re_probe);
            entry->second = arp_probe_ttl;
        } else {
            entry->second -= ms_since_last_tick;
        }
    }
}

void NetworkInterface::_update_arp_table(initializer_list<ArpEntry> arp_entry) {
    for (auto &entry : arp_entry) {
        _arp_table[entry] = arp_max_ttl;
    }
}