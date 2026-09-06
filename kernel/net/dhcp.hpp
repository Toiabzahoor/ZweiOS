#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

struct DHCPPacket {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    char     sname[64];
    char     file[128];
    uint32_t magic_cookie;
    uint8_t  options[308];
} __attribute__((packed));

void dhcp_init();
bool dhcp_discover(uint32_t timeout_ms = 3000);
bool dhcp_is_configured();

}
