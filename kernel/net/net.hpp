#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

struct InterfaceConfig {
    char     name[16];
    uint8_t  mac[6];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    bool     up;
    bool     dhcp_enabled;
};

void            net_init();
void            net_poll();
bool            net_is_up();

InterfaceConfig net_get_config();
void            net_set_ip(uint32_t ip);
void            net_set_netmask(uint32_t mask);
void            net_set_gateway(uint32_t gw);
void            net_set_dns(uint32_t dns);

void            net_get_mac(uint8_t out_mac[6]);

}
