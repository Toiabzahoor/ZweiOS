#include "net/arp.hpp"
#include "net/ethernet.hpp"
#include "net/net.hpp"
#include "lib/string.hpp"

namespace net {

struct ArpEntry {
    uint32_t ip;
    uint8_t  mac[6];
    bool     valid;
};

static constexpr size_t ARP_CACHE_SIZE = 64;
static ArpEntry g_arp_cache[ARP_CACHE_SIZE];

static const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void arp_init() {
    for (size_t i = 0; i < ARP_CACHE_SIZE; ++i) {
        g_arp_cache[i].ip = 0;
        g_arp_cache[i].valid = false;
        lib::memset(g_arp_cache[i].mac, 0, 6);
    }
}

void arp_insert(uint32_t ip, const uint8_t mac[6]) {
    if (!mac || ip == 0) return;

    for (size_t i = 0; i < ARP_CACHE_SIZE; ++i) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            lib::memcpy(g_arp_cache[i].mac, mac, 6);
            return;
        }
    }

    for (size_t i = 0; i < ARP_CACHE_SIZE; ++i) {
        if (!g_arp_cache[i].valid) {
            g_arp_cache[i].ip = ip;
            lib::memcpy(g_arp_cache[i].mac, mac, 6);
            g_arp_cache[i].valid = true;
            return;
        }
    }

    g_arp_cache[0].ip = ip;
    lib::memcpy(g_arp_cache[0].mac, mac, 6);
    g_arp_cache[0].valid = true;
}

bool arp_lookup(uint32_t ip, uint8_t out_mac[6]) {
    if (!out_mac || ip == 0) return false;

    if (ip == 0xFFFFFFFFU) {
        lib::memcpy(out_mac, BROADCAST_MAC, 6);
        return true;
    }

    for (size_t i = 0; i < ARP_CACHE_SIZE; ++i) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            lib::memcpy(out_mac, g_arp_cache[i].mac, 6);
            return true;
        }
    }

    return false;
}

bool arp_send_request(uint32_t target_ip) {
    InterfaceConfig cfg = net_get_config();

    ArpPacket pkt;
    pkt.hw_type = htons(ARP_HW_ETHERNET);
    pkt.proto_type = htons(ARP_PROTO_IPV4);
    pkt.hw_len = 6;
    pkt.proto_len = 4;
    pkt.opcode = htons(ARP_OP_REQUEST);

    net_get_mac(pkt.src_mac);
    pkt.src_ip = cfg.ip;
    lib::memset(pkt.dst_mac, 0, 6);
    pkt.dst_ip = target_ip;

    return ethernet_send(BROADCAST_MAC, ETHERTYPE_ARP, reinterpret_cast<const uint8_t*>(&pkt), sizeof(ArpPacket));
}

void arp_handle_packet(const uint8_t* payload, size_t len) {
    if (!payload || len < sizeof(ArpPacket)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const ArpPacket*>(payload);

    if (ntohs(pkt->hw_type) != ARP_HW_ETHERNET || ntohs(pkt->proto_type) != ARP_PROTO_IPV4) {
        return;
    }

    uint16_t op = ntohs(pkt->opcode);
    arp_insert(pkt->src_ip, pkt->src_mac);

    InterfaceConfig cfg = net_get_config();

    if (op == ARP_OP_REQUEST && pkt->dst_ip == cfg.ip) {
        ArpPacket reply;
        reply.hw_type = htons(ARP_HW_ETHERNET);
        reply.proto_type = htons(ARP_PROTO_IPV4);
        reply.hw_len = 6;
        reply.proto_len = 4;
        reply.opcode = htons(ARP_OP_REPLY);

        net_get_mac(reply.src_mac);
        reply.src_ip = cfg.ip;
        lib::memcpy(reply.dst_mac, pkt->src_mac, 6);
        reply.dst_ip = pkt->src_ip;

        ethernet_send(pkt->src_mac, ETHERTYPE_ARP, reinterpret_cast<const uint8_t*>(&reply), sizeof(ArpPacket));
    }
}

}
