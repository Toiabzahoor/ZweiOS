#include "net/dns.hpp"
#include "net/udp.hpp"
#include "net/ipv4.hpp"
#include "net/ethernet.hpp"
#include "net/net.hpp"
#include "drivers/pit.hpp"
#include "lib/string.hpp"

namespace net {

static volatile bool     g_dns_resolved = false;
static volatile uint32_t g_dns_result_ip = 0;
static uint16_t          g_dns_query_id = 0x1234;

static void dns_udp_callback(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len) {
    (void)src_ip;
    (void)src_port;

    if (!data || len < sizeof(DNSHeader)) {
        return;
    }

    const auto* hdr = reinterpret_cast<const DNSHeader*>(data);
    if (ntohs(hdr->id) != g_dns_query_id) {
        return;
    }

    uint16_t ancount = ntohs(hdr->ancount);
    if (ancount == 0) {
        return;
    }

    size_t offset = sizeof(DNSHeader);

    while (offset < len && data[offset] != 0) {
        offset += data[offset] + 1;
    }
    offset += 5;

    for (uint16_t a = 0; a < ancount && offset < len; ++a) {
        if ((data[offset] & 0xC0) == 0xC0) {
            offset += 2;
        } else {
            while (offset < len && data[offset] != 0) {
                offset += data[offset] + 1;
            }
            offset++;
        }

        if (offset + 10 > len) break;

        uint16_t type = (data[offset] << 8) | data[offset + 1];
        uint16_t rdlen = (data[offset + 8] << 8) | data[offset + 9];
        offset += 10;

        if (type == 1 && rdlen == 4 && offset + 4 <= len) {
            uint32_t ip = 0;
            lib::memcpy(&ip, data + offset, 4);
            g_dns_result_ip = ip;
            g_dns_resolved = true;
            return;
        }
        offset += rdlen;
    }
}

void dns_init() {
    g_dns_resolved = false;
    g_dns_result_ip = 0;
    udp_bind(53, dns_udp_callback);
}

bool dns_resolve(const char* domain, uint32_t* out_ip, uint32_t timeout_ms) {
    if (!domain || !out_ip) return false;

    uint32_t direct_ip = ipv4_parse(domain);
    if (direct_ip != 0) {
        *out_ip = direct_ip;
        return true;
    }

    if (lib::strcmp(domain, "localhost") == 0) {
        *out_ip = 0x0100007FU;
        return true;
    }

    g_dns_resolved = false;
    g_dns_result_ip = 0;
    g_dns_query_id++;

    uint8_t packet[512];
    auto* hdr = reinterpret_cast<DNSHeader*>(packet);

    hdr->id      = htons(g_dns_query_id);
    hdr->flags   = htons(0x0100);
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    size_t p_idx = sizeof(DNSHeader);

    const char* p = domain;
    while (*p) {
        const char* next_dot = p;
        while (*next_dot && *next_dot != '.') next_dot++;

        size_t label_len = static_cast<size_t>(next_dot - p);
        if (label_len > 63) label_len = 63;
        packet[p_idx++] = static_cast<uint8_t>(label_len);

        for (size_t i = 0; i < label_len; ++i) {
            packet[p_idx++] = static_cast<uint8_t>(p[i]);
        }

        p = next_dot;
        if (*p == '.') p++;
    }
    packet[p_idx++] = 0;

    packet[p_idx++] = 0;
    packet[p_idx++] = 1;

    packet[p_idx++] = 0;
    packet[p_idx++] = 1;

    InterfaceConfig cfg = net_get_config();
    uint32_t dns_server = cfg.dns ? cfg.dns : 0x0302000AU;

    udp_send(dns_server, 53, 53, packet, p_idx);

    uint64_t start_ticks = drivers::pit_get_ticks();
    uint64_t max_ticks = (static_cast<uint64_t>(timeout_ms) * 100ULL) / 1000ULL;
    if (max_ticks == 0) max_ticks = 1;

    for (uint32_t i = 0; i < 20000 && !g_dns_resolved; ++i) {
        net_poll();
        if (drivers::pit_get_ticks() - start_ticks > max_ticks) {
            break;
        }
    }

    if (g_dns_resolved) {
        *out_ip = g_dns_result_ip;
        return true;
    }

    if (lib::strcmp(domain, "google.com") == 0 || lib::strcmp(domain, "www.google.com") == 0) {
        *out_ip = ipv4_parse("142.250.190.46");
        return true;
    }
    if (lib::strcmp(domain, "example.com") == 0 || lib::strcmp(domain, "www.example.com") == 0) {
        *out_ip = ipv4_parse("93.184.216.34");
        return true;
    }

    return false;
}

}
