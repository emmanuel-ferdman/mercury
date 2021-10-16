/*
 * proto_identify.h
 *
 * Copyright (c) 2019 Cisco Systems, Inc. All rights reserved.  License at
 * https://github.com/cisco/mercury/blob/master/LICENSE
 */

/**
 * \file proto_identify.h
 *
 * \brief Protocol identification (header)
 */

#ifndef PROTO_IDENTIFY_H
#define PROTO_IDENTIFY_H

#include <stdint.h>
#include <vector>
#include <array>
#include "match.h"

#include "tls.h"   // tcp protocols
#include "http.h"
#include "ssh.h"
#include "smtp.h"

//#include "dhcp.h"  // udp protocols
#include "quic.h"
#include "dns.h"
#include "wireguard.h"

enum tcp_msg_type {
    tcp_msg_type_unknown = 0,
    tcp_msg_type_http_request,
    tcp_msg_type_http_response,
    tcp_msg_type_tls_client_hello,
    tcp_msg_type_tls_server_hello,
    tcp_msg_type_tls_certificate,
    tcp_msg_type_ssh,
    tcp_msg_type_ssh_kex,
    tcp_msg_type_smtp_client,
    tcp_msg_type_smtp_server,
};

enum udp_msg_type {
    udp_msg_type_unknown = 0,
    udp_msg_type_dns,
    udp_msg_type_dhcp,
    udp_msg_type_dtls_client_hello,
    udp_msg_type_dtls_server_hello,
    udp_msg_type_dtls_certificate,
    udp_msg_type_wireguard,
    udp_msg_type_quic,
    udp_msg_type_vxlan
};

template <size_t N>
struct matcher_and_type {
    mask_and_value<N> mv;
    size_t type;
};


template <size_t N>
class protocol_identifier {
    std::vector<matcher_and_type<N>> short_matchers;

public:

    protocol_identifier() : short_matchers{} {  }

    void add_protocol(const mask_and_value<N> &mv, size_t type) {
        struct matcher_and_type<N> new_proto{mv, type};
        short_matchers.push_back(new_proto);
    }

    void compile() {
        // this function is a placeholder for now, but in the future,
        // it may compile a jump table, reorder matchers, etc.
    }

    size_t get_msg_type(const uint8_t *data, unsigned int len) {

        // TODO: process short data fields
        //
        if (len < 8) {
            return 0;   // type unknown;
        }
        for (matcher_and_type p : short_matchers) {
            if (p.mv.matches(data)) {
                return p.type;
            }
        }
        return 0;   // type unknown;
    }

};

bool set_config(std::map<std::string, bool> &config_map, const char *config_string); // in pkt_proc.cc

// class selector implements a protocol selection policy for TCP and
// UDP traffic
//
class traffic_selector {
    protocol_identifier<8> tcp;
    protocol_identifier<8> udp;

public:

    traffic_selector(const char *config_string) : tcp{}, udp{} {

        // a null configuration string defaults to all protocols
        //
        const char *all = "all";
        if (config_string == nullptr) {
            config_string = all;
        }
        // create a map of protocol names and booleans, then update it
        // based on the configuration string
        //
        std::map<std::string, bool> protocols{
            { "all",         false },
            { "none",        false },
            { "dhcp",        false },
            { "dns",         false },
            { "dtls",        false },
            { "http",        false },
            { "ssh",         false },
            { "tcp",         false },
            { "tcp.message", false },
            { "tls",         false },
            { "wireguard",   false },
            { "quic",        false },
            { "smtp",        false },
        };
        if (!set_config(protocols, config_string)) {
            throw std::runtime_error("error: could not parse protocol identification configuration string");
        }

        // "none" is a special case; turn off all protocol selection
        //
        if (protocols["none"]) {
            for (auto &pair : protocols) {
                pair.second = false;
            }
        }

        if (protocols["tls"] || protocols["all"]) {
            tcp.add_protocol(tls_client_hello::matcher, tcp_msg_type_tls_client_hello);
            tcp.add_protocol(tls_server_hello::matcher, tcp_msg_type_tls_server_hello);
        }
        if (protocols["http"] || protocols["all"]) {
            tcp.add_protocol(http_request::get_matcher, tcp_msg_type_http_request);
            tcp.add_protocol(http_request::post_matcher, tcp_msg_type_http_request);
            tcp.add_protocol(http_request::connect_matcher, tcp_msg_type_http_request);
            tcp.add_protocol(http_request::put_matcher, tcp_msg_type_http_request);
            tcp.add_protocol(http_request::head_matcher, tcp_msg_type_http_request);
            tcp.add_protocol(http_response::matcher, tcp_msg_type_http_response);
        }
        if (protocols["ssh"] || protocols["all"]) {
            tcp.add_protocol(ssh_init_packet::matcher, tcp_msg_type_ssh);
            tcp.add_protocol(ssh_kex_init::matcher, tcp_msg_type_ssh_kex);
        }
        if (protocols["smtp"] || protocols["all"]) {
            tcp.add_protocol(smtp_client::matcher, tcp_msg_type_smtp_client);
            tcp.add_protocol(smtp_server::matcher, tcp_msg_type_smtp_server);
        }

        // UDP and booleans not yet implemented
        //
        if (protocols["tcp"]) {
            // select_tcp_syn = 0;
        }
        if (protocols["tcp.message"]) {
            // select_tcp_syn = 0;
            // tcp_message_filter_cutoff = 1;
        }
        if (protocols["dhcp"]) {
            //udp.add_protocol(dhcp_discover::matcher, udp_msg_type_dhcp);
        }
        if (protocols["dns"]) {
            // select_mdns = false;
            udp.add_protocol(dns_packet::matcher, udp_msg_type_dns);
        }
        if (protocols["dtls"]) {
            ;
        }
        if (protocols["wireguard"]) {
            ;
        }
        if (protocols["quic"]) {
            ;
        }

        // tell protocol_identification objects to compile lookup tables
        //
        tcp.compile();

    }

    size_t get_tcp_msg_type(const uint8_t *data, unsigned int len) {
        return tcp.get_msg_type(data, len);
    }

};

#endif /* PROTO_IDENTIFY_H */
