#include "net/net.hpp"
#include "net/ethernet.hpp"
#include "net/arp.hpp"
#include "net/ipv4.hpp"
#include "net/icmp.hpp"
#include "net/udp.hpp"
#include "net/tcp.hpp"
#include "net/dhcp.hpp"
#include "net/dns.hpp"
#include "net/socket.hpp"
#include "drivers/pci.hpp"
#include "drivers/e1000.hpp"
#include "drivers/serial.hpp"
#include "lib/string.hpp"

namespace net {

static InterfaceConfig g_if_config;

void net_init() {
    lib::strncpy(g_if_config.name, "eth0", sizeof(g_if_config.name));
    lib::memset(g_if_config.mac, 0, 6);
    g_if_config.ip = 0;
    g_if_config.netmask = 0;
    g_if_config.gateway = 0;
    g_if_config.dns = 0;
    g_if_config.up = false;
    g_if_config.dhcp_enabled = true;

    drivers::pci_init();

    if (drivers::e1000_init()) {
        drivers::e1000_get_mac(g_if_config.mac);
        g_if_config.up = true;
    }

    arp_init();
    icmp_init();
    udp_init();
    tcp_init();
    dhcp_init();
    dns_init();
    sock_init();

    if (g_if_config.up) {
        drivers::serial_puts("[NET] Initializing DHCP configuration...\r\n");
        dhcp_discover(2000);
    }

    drivers::serial_puts("[NET] Gigabit Ethernet & TCP/IP network stack initialized.\r\n");
}

void net_poll() {
    if (!g_if_config.up) return;

    uint8_t pkt_buf[2048];
    size_t len = drivers::e1000_receive_packet(pkt_buf, sizeof(pkt_buf));
    while (len > 0) {
        ethernet_handle_packet(pkt_buf, len);
        len = drivers::e1000_receive_packet(pkt_buf, sizeof(pkt_buf));
    }
}

bool net_is_up() {
    return g_if_config.up;
}

InterfaceConfig net_get_config() {
    return g_if_config;
}

void net_set_ip(uint32_t ip) {
    g_if_config.ip = ip;
}

void net_set_netmask(uint32_t mask) {
    g_if_config.netmask = mask;
}

void net_set_gateway(uint32_t gw) {
    g_if_config.gateway = gw;
}

void net_set_dns(uint32_t dns) {
    g_if_config.dns = dns;
}

void net_get_mac(uint8_t out_mac[6]) {
    if (!out_mac) return;
    for (size_t i = 0; i < 6; ++i) {
        out_mac[i] = g_if_config.mac[i];
    }
}

}
