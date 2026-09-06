#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

inline constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
inline constexpr uint16_t ETHERTYPE_ARP  = 0x0806;

inline constexpr uint16_t swap16(uint16_t x) {
    return static_cast<uint16_t>((x << 8) | (x >> 8));
}

inline constexpr uint32_t swap32(uint32_t x) {
    return ((x & 0x000000FFU) << 24)
         | ((x & 0x0000FF00U) << 8)
         | ((x & 0x00FF0000U) >> 8)
         | ((x & 0xFF000000U) >> 24);
}

inline constexpr uint16_t htons(uint16_t x) { return swap16(x); }
inline constexpr uint16_t ntohs(uint16_t x) { return swap16(x); }
inline constexpr uint32_t htonl(uint32_t x) { return swap32(x); }
inline constexpr uint32_t ntohl(uint32_t x) { return swap32(x); }

struct EthernetHeader {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} __attribute__((packed));

bool ethernet_send(const uint8_t dest_mac[6], uint16_t ethertype, const uint8_t* payload, size_t payload_len);
void ethernet_handle_packet(const uint8_t* frame, size_t len);

}
