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
    auto matched_entry = _route_table.end();
    for (auto entry = _route_table.begin(); entry != _route_table.end(); entry++) {
        if (entry->route_prefix == 0 ||
            (entry->route_prefix ^ dgram.header().dst) >> (32 - entry->prefix_length) == 0) {
            if (matched_entry == _route_table.end() || matched_entry->prefix_length < entry->prefix_length) {
                matched_entry = entry;
            }
        }
    }

    // 检查是否存在对应的路由规则，不存在则直接抛弃
    if (matched_entry == _route_table.end()) {
        return;
    }

    // 如果数据包的TTL减少到了0，则直接丢弃
    if (dgram.header().ttl-- <= 1) {
        return;
    }

    // 将数据包发送给正确的接口
    AsyncNetworkInterface &interface = _interfaces[matched_entry->interface_num];
    if (matched_entry->next_hop.has_value()) {
        interface.send_datagram(dgram, matched_entry->next_hop.value());
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
