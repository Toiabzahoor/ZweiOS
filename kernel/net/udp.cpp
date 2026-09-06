#include "net/udp.hpp"
#include "net/ipv4.hpp"
#include "net/ethernet.hpp"
#include "net/net.hpp"
#include "lib/string.hpp"

namespace net {

struct UDPBinding {
    uint16_t   port;
    UDPHandler handler;
    bool       in_use;
};

static constexpr size_t MAX_UDP_BINDINGS = 32;
static UDPBinding g_udp_bindings[MAX_UDP_BINDINGS];

void udp_init() {
    for (size_t i = 0; i < MAX_UDP_BINDINGS; ++i) {
        g_udp_bindings[i].port = 0;
        g_udp_bindings[i].handler = nullptr;
        g_udp_bindings[i].in_use = false;
    }
}

bool udp_bind(uint16_t port, UDPHandler handler) {
    if (!handler || port == 0) return false;

    for (size_t i = 0; i < MAX_UDP_BINDINGS; ++i) {
        if (g_udp_bindings[i].in_use && g_udp_bindings[i].port == port) {
            g_udp_bindings[i].handler = handler;
            return true;
        }
    }

    for (size_t i = 0; i < MAX_UDP_BINDINGS; ++i) {
        if (!g_udp_bindings[i].in_use) {
            g_udp_bindings[i].port = port;
            g_udp_bindings[i].handler = handler;
            g_udp_bindings[i].in_use = true;
            return true;
        }
    }

    return false;
}

void udp_unbind(uint16_t port) {
    for (size_t i = 0; i < MAX_UDP_BINDINGS; ++i) {
        if (g_udp_bindings[i].in_use && g_udp_bindings[i].port == port) {
            g_udp_bindings[i].in_use = false;
            g_udp_bindings[i].handler = nullptr;
            return;
        }
    }
}

bool udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const uint8_t* payload, size_t len) {
    if (len > 1400) return false;

    uint8_t packet[1500];
    auto* hdr = reinterpret_cast<UDPHeader*>(packet);

    uint16_t udp_total_len = static_cast<uint16_t>(sizeof(UDPHeader) + len);
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length   = htons(udp_total_len);
    hdr->checksum = 0;

    if (payload && len > 0) {
        lib::memcpy(packet + sizeof(UDPHeader), payload, len);
    }

    InterfaceConfig cfg = net_get_config();

    struct PseudoHeader {
        uint32_t src_ip;
        uint32_t dst_ip;
        uint8_t  zero;
        uint8_t  protocol;
        uint16_t udp_len;
    } __attribute__((packed));

    uint8_t pseudo_buf[1500];
    auto* phdr = reinterpret_cast<PseudoHeader*>(pseudo_buf);
    phdr->src_ip = cfg.ip;
    phdr->dst_ip = dst_ip;
    phdr->zero = 0;
    phdr->protocol = IP_PROTO_UDP;
    phdr->udp_len = htons(udp_total_len);

    lib::memcpy(pseudo_buf + sizeof(PseudoHeader), packet, udp_total_len);
    hdr->checksum = ipv4_checksum(pseudo_buf, sizeof(PseudoHeader) + udp_total_len);
    if (hdr->checksum == 0) hdr->checksum = 0xFFFF;

    return ipv4_send(dst_ip, IP_PROTO_UDP, packet, udp_total_len);
}

void udp_handle_packet(uint32_t src_ip, const uint8_t* payload, size_t len) {
    if (!payload || len < sizeof(UDPHeader)) {
        return;
    }

    const auto* hdr = reinterpret_cast<const UDPHeader*>(payload);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t u_len    = ntohs(hdr->length);

    if (len < u_len || u_len < sizeof(UDPHeader)) {
        return;
    }

    const uint8_t* data = payload + sizeof(UDPHeader);
    size_t data_len = u_len - sizeof(UDPHeader);

    for (size_t i = 0; i < MAX_UDP_BINDINGS; ++i) {
        if (g_udp_bindings[i].in_use && g_udp_bindings[i].port == dst_port) {
            if (g_udp_bindings[i].handler) {
                g_udp_bindings[i].handler(src_ip, src_port, data, data_len);
            }
            break;
        }
    }
}

}
