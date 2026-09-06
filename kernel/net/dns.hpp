#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

struct DNSHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

void dns_init();
bool dns_resolve(const char* domain, uint32_t* out_ip, uint32_t timeout_ms = 3000);

}
