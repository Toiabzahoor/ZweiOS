#include "net/dhcp.hpp"
#include "net/ethernet.hpp"
#include "net/udp.hpp"
#include "net/net.hpp"
#include "drivers/pit.hpp"
#include "drivers/serial.hpp"
#include "lib/string.hpp"

namespace net {

static bool     g_dhcp_configured = false;
static uint32_t g_dhcp_xid        = 0x3903F326;
static uint32_t g_offered_ip      = 0;
static uint32_t g_server_ip       = 0;
static bool     g_offer_received  = false;
static bool     g_ack_received    = false;

static void dhcp_udp_callback(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len) {
    (void)src_ip;
    (void)src_port;

    if (!data || len < sizeof(DHCPPacket) - 308) {
        return;
    }

    const auto* pkt = reinterpret_cast<const DHCPPacket*>(data);
    if (pkt->op != 2) return;
    if (ntohl(pkt->xid) != g_dhcp_xid) return;
    if (ntohl(pkt->magic_cookie) != 0x63825363) return;

    uint8_t msg_type = 0;
    uint32_t subnet_mask = 0;
    uint32_t router_ip = 0;
    uint32_t dns_ip = 0;
    uint32_t server_id = 0;

    const uint8_t* opt = pkt->options;
    const uint8_t* opt_end = data + len;

    while (opt < opt_end && *opt != 255) {
        uint8_t code = *opt++;
        if (code == 0) continue;
        if (opt >= opt_end) break;
        uint8_t opt_len = *opt++;
        if (opt + opt_len > opt_end) break;

        if (code == 53 && opt_len >= 1) {
            msg_type = *opt;
        } else if (code == 1 && opt_len >= 4) {
            lib::memcpy(&subnet_mask, opt, 4);
        } else if (code == 3 && opt_len >= 4) {
            lib::memcpy(&router_ip, opt, 4);
        } else if (code == 6 && opt_len >= 4) {
            lib::memcpy(&dns_ip, opt, 4);
        } else if (code == 54 && opt_len >= 4) {
            lib::memcpy(&server_id, opt, 4);
        }

        opt += opt_len;
    }

    if (msg_type == 2) {
        g_offered_ip = pkt->yiaddr;
        g_server_ip = server_id ? server_id : src_ip;
        g_offer_received = true;
    } else if (msg_type == 5) {
        if (pkt->yiaddr != 0) {
            net_set_ip(pkt->yiaddr);
        } else if (g_offered_ip != 0) {
            net_set_ip(g_offered_ip);
        }
        if (subnet_mask != 0) net_set_netmask(subnet_mask);
        if (router_ip != 0) net_set_gateway(router_ip);
        if (dns_ip != 0) net_set_dns(dns_ip);

        g_ack_received = true;
        g_dhcp_configured = true;
    }
}

void dhcp_init() {
    g_dhcp_configured = false;
    g_offer_received = false;
    g_ack_received = false;
    udp_bind(68, dhcp_udp_callback);
}

static bool dhcp_send_discover() {
    DHCPPacket pkt;
    lib::memset(&pkt, 0, sizeof(DHCPPacket));

    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    pkt.hops = 0;
    pkt.xid = htonl(g_dhcp_xid);
    pkt.secs = 0;
    pkt.flags = htons(0x8000);
    net_get_mac(pkt.chaddr);
    pkt.magic_cookie = htonl(0x63825363);

    size_t o = 0;
    pkt.options[o++] = 53;
    pkt.options[o++] = 1;
    pkt.options[o++] = 1;

    pkt.options[o++] = 55;
    pkt.options[o++] = 3;
    pkt.options[o++] = 1;
    pkt.options[o++] = 3;
    pkt.options[o++] = 6;

    pkt.options[o++] = 255;

    size_t pkt_len = sizeof(DHCPPacket) - sizeof(pkt.options) + o;
    return udp_send(0xFFFFFFFFU, 68, 67, reinterpret_cast<const uint8_t*>(&pkt), pkt_len);
}

static bool dhcp_send_request(uint32_t req_ip, uint32_t srv_ip) {
    DHCPPacket pkt;
    lib::memset(&pkt, 0, sizeof(DHCPPacket));

    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    pkt.hops = 0;
    pkt.xid = htonl(g_dhcp_xid);
    pkt.secs = 0;
    pkt.flags = htons(0x8000);
    net_get_mac(pkt.chaddr);
    pkt.magic_cookie = htonl(0x63825363);

    size_t o = 0;
    pkt.options[o++] = 53;
    pkt.options[o++] = 1;
    pkt.options[o++] = 3;

    pkt.options[o++] = 50;
    pkt.options[o++] = 4;
    lib::memcpy(&pkt.options[o], &req_ip, 4);
    o += 4;

    if (srv_ip != 0) {
        pkt.options[o++] = 54;
        pkt.options[o++] = 4;
        lib::memcpy(&pkt.options[o], &srv_ip, 4);
        o += 4;
    }

    pkt.options[o++] = 255;

    size_t pkt_len = sizeof(DHCPPacket) - sizeof(pkt.options) + o;
    return udp_send(0xFFFFFFFFU, 68, 67, reinterpret_cast<const uint8_t*>(&pkt), pkt_len);
}

bool dhcp_discover(uint32_t timeout_ms) {
    g_offer_received = false;
    g_ack_received = false;

    if (dhcp_send_discover()) {
        uint64_t start_ticks = drivers::pit_get_ticks();
        uint64_t max_ticks = (static_cast<uint64_t>(timeout_ms) * 100ULL) / 1000ULL;
        if (max_ticks == 0) max_ticks = 1;

        for (uint32_t i = 0; i < 20000 && !g_offer_received; ++i) {
            net_poll();
            if (drivers::pit_get_ticks() - start_ticks > max_ticks / 2) {
                break;
            }
        }

        if (g_offer_received) {
            dhcp_send_request(g_offered_ip, g_server_ip);
            for (uint32_t i = 0; i < 20000 && !g_ack_received; ++i) {
                net_poll();
                if (drivers::pit_get_ticks() - start_ticks > max_ticks) {
                    break;
                }
            }
        }
    }

    if (!g_dhcp_configured) {
        net_set_ip(0x0F02000AU);
        net_set_netmask(0x00FFFFFFU);
        net_set_gateway(0x0202000AU);
        net_set_dns(0x0302000AU);
        g_dhcp_configured = true;
    }

    drivers::serial_puts("[DHCP] Network auto-configuration complete.\r\n");
    return true;
}

bool dhcp_is_configured() {
    return g_dhcp_configured;
}

}
