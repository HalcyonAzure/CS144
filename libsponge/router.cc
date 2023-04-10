#include "router.hh"

#include <cstdint>
#include <iostream>
#include <optional>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // 添加路由表
    _route_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    auto path = _route_table.end();
    const auto &dst_ip = dgram.header().dst;
    for (auto entry = _route_table.begin(); entry != _route_table.end(); entry++) {
        // CIDR的子网位数是多少，相当于就是在0的基础上补多少个1，但是当prefix_length == 0的时候，
        // 由于位运算的特性，子网掩码会全部变成1，也就相当于是/32的情况。因此当检测到子网掩码是0的时候要直接跳过
        const uint32_t &mask = entry->prefix_length ? (~0U) << (32 - entry->prefix_length) : 0;
        const auto network_address = entry->route_prefix & mask;
        if ((dst_ip & mask) == network_address) {
            path = entry;
        }
    }

    // 检查是否存在对应的路由规则，或者TTL可否生存，如果不符合则丢弃
    if (path == _route_table.end() || dgram.header().ttl-- <= 1) {
        return;
    }

    // 将数据包发送给正确的接口
    AsyncNetworkInterface &interface = _interfaces[path->interface_num];
    if (path->next_hop.has_value()) {
        interface.send_datagram(dgram, path->next_hop.value());
    } else {
        interface.send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
