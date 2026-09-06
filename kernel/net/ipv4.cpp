#include "net/ipv4.hpp"
#include "net/ethernet.hpp"
#include "net/arp.hpp"
#include "net/icmp.hpp"
#include "net/udp.hpp"
#include "net/tcp.hpp"
#include "net/net.hpp"
#include "lib/string.hpp"

namespace net {

static uint16_t g_ip_packet_id = 1;

uint16_t ipv4_checksum(const void* data, size_t len) {
    auto* ptr = reinterpret_cast<const uint16_t*>(data);
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len > 0) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

bool ipv4_send(uint32_t dst_ip, uint8_t protocol, const uint8_t* payload, size_t payload_len) {
    if (!payload || payload_len == 0 || payload_len > 1480) {
        return false;
    }

    InterfaceConfig cfg = net_get_config();

    uint8_t dst_mac[6];
    if (dst_ip == 0xFFFFFFFFU) {
        lib::memset(dst_mac, 0xFF, 6);
    } else {
        uint32_t next_hop = dst_ip;
        if ((dst_ip & cfg.netmask) != (cfg.ip & cfg.netmask)) {
            next_hop = cfg.gateway;
        }

        if (!arp_lookup(next_hop, dst_mac)) {
            arp_send_request(next_hop);
            for (volatile size_t i = 0; i < 50000; ++i) {
                net_poll();
                if (arp_lookup(next_hop, dst_mac)) break;
            }
            if (!arp_lookup(next_hop, dst_mac)) {
                return false;
            }
        }
    }

    uint8_t packet[1500];
    auto* hdr = reinterpret_cast<IPv4Header*>(packet);

    hdr->ver_ihl = 0x45;
    hdr->tos = 0;
    hdr->total_length = htons(static_cast<uint16_t>(sizeof(IPv4Header) + payload_len));
    hdr->id = htons(g_ip_packet_id++);
    hdr->flags_fragment = htons(0x4000);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = cfg.ip;
    hdr->dst_ip = dst_ip;

    hdr->checksum = ipv4_checksum(hdr, sizeof(IPv4Header));

    lib::memcpy(packet + sizeof(IPv4Header), payload, payload_len);

    return ethernet_send(dst_mac, ETHERTYPE_IPV4, packet, sizeof(IPv4Header) + payload_len);
}

void ipv4_handle_packet(const uint8_t* payload, size_t len) {
    if (!payload || len < sizeof(IPv4Header)) {
        return;
    }

    const auto* hdr = reinterpret_cast<const IPv4Header*>(payload);

    uint8_t ver = (hdr->ver_ihl >> 4) & 0x0F;
    uint8_t ihl = (hdr->ver_ihl & 0x0F) * 4;

    if (ver != 4 || ihl < sizeof(IPv4Header) || len < ihl) {
        return;
    }

    uint16_t total_len = ntohs(hdr->total_length);
    if (len < total_len) {
        return;
    }

    if (ipv4_checksum(hdr, ihl) != 0) {
        return;
    }

    InterfaceConfig cfg = net_get_config();
    if (hdr->dst_ip != cfg.ip && hdr->dst_ip != 0xFFFFFFFFU && cfg.ip != 0) {
        return;
    }

    const uint8_t* proto_payload = payload + ihl;
    size_t proto_len = total_len - ihl;

    if (hdr->protocol == IP_PROTO_ICMP) {
        icmp_handle_packet(hdr->src_ip, proto_payload, proto_len);
    } else if (hdr->protocol == IP_PROTO_UDP) {
        udp_handle_packet(hdr->src_ip, proto_payload, proto_len);
    } else if (hdr->protocol == IP_PROTO_TCP) {
        tcp_handle_packet(hdr->src_ip, proto_payload, proto_len);
    }
}

uint32_t ipv4_parse(const char* str) {
    if (!str) return 0;

    uint32_t bytes[4] = { 0, 0, 0, 0 };
    size_t byte_idx = 0;

    while (*str && byte_idx < 4) {
        if (*str >= '0' && *str <= '9') {
            bytes[byte_idx] = (bytes[byte_idx] * 10) + static_cast<uint32_t>(*str - '0');
            if (bytes[byte_idx] > 255) return 0;
        } else if (*str == '.') {
            byte_idx++;
        } else {
            break;
        }
        str++;
    }

    if (byte_idx != 3) return 0;

    return (bytes[0]) | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

void ipv4_format(uint32_t ip, char* out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return;

    uint8_t b0 = static_cast<uint8_t>(ip & 0xFF);
    uint8_t b1 = static_cast<uint8_t>((ip >> 8) & 0xFF);
    uint8_t b2 = static_cast<uint8_t>((ip >> 16) & 0xFF);
    uint8_t b3 = static_cast<uint8_t>((ip >> 24) & 0xFF);

    size_t pos = 0;
    auto append_num = [&](uint8_t val) {
        char tmp[4];
        size_t t_len = 0;
        if (val == 0) tmp[t_len++] = '0';
        else {
            char rev[4];
            size_t r = 0;
            while (val > 0) { rev[r++] = static_cast<char>('0' + (val % 10)); val /= 10; }
            while (r > 0) tmp[t_len++] = rev[--r];
        }
        for (size_t k = 0; k < t_len && pos < max_len - 1; ++k) {
            out_buf[pos++] = tmp[k];
        }
    };

    append_num(b0);
    if (pos < max_len - 1) out_buf[pos++] = '.';
    append_num(b1);
    if (pos < max_len - 1) out_buf[pos++] = '.';
    append_num(b2);
    if (pos < max_len - 1) out_buf[pos++] = '.';
    append_num(b3);

    out_buf[pos] = '\0';
}

}
