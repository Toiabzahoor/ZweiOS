#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

inline constexpr uint8_t ICMP_TYPE_ECHO_REPLY   = 0;
inline constexpr uint8_t ICMP_TYPE_ECHO_REQUEST = 8;

struct ICMPHeader {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed));

void icmp_init();
void icmp_handle_packet(uint32_t src_ip, const uint8_t* payload, size_t len);
bool icmp_send_echo_request(uint32_t dst_ip, uint16_t id, uint16_t seq, const uint8_t* data, size_t data_len);
bool icmp_ping(uint32_t dst_ip, uint32_t timeout_ms, uint32_t* out_rtt_ms);

}
