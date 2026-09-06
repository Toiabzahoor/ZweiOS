#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

inline constexpr uint8_t IP_PROTO_ICMP = 1;
inline constexpr uint8_t IP_PROTO_TCP  = 6;
inline constexpr uint8_t IP_PROTO_UDP  = 17;

struct IPv4Header {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

uint16_t ipv4_checksum(const void* data, size_t len);
bool     ipv4_send(uint32_t dst_ip, uint8_t protocol, const uint8_t* payload, size_t payload_len);
void     ipv4_handle_packet(const uint8_t* payload, size_t len);

uint32_t ipv4_parse(const char* str);
void     ipv4_format(uint32_t ip, char* out_buf, size_t max_len);

}
