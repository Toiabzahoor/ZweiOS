#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

inline constexpr uint16_t ARP_HW_ETHERNET = 1;
inline constexpr uint16_t ARP_PROTO_IPV4  = 0x0800;
inline constexpr uint16_t ARP_OP_REQUEST  = 1;
inline constexpr uint16_t ARP_OP_REPLY    = 2;

struct ArpPacket {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dst_mac[6];
    uint32_t dst_ip;
} __attribute__((packed));

void arp_init();
void arp_handle_packet(const uint8_t* payload, size_t len);
bool arp_send_request(uint32_t target_ip);
bool arp_lookup(uint32_t ip, uint8_t out_mac[6]);
void arp_insert(uint32_t ip, const uint8_t mac[6]);

}
