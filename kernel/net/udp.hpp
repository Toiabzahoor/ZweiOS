#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

struct UDPHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

typedef void (*UDPHandler)(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len);

void udp_init();
bool udp_bind(uint16_t port, UDPHandler handler);
void udp_unbind(uint16_t port);
bool udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const uint8_t* payload, size_t len);
void udp_handle_packet(uint32_t src_ip, const uint8_t* payload, size_t len);

}
