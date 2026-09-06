#include "net/ethernet.hpp"
#include "net/arp.hpp"
#include "net/ipv4.hpp"
#include "net/net.hpp"
#include "drivers/e1000.hpp"
#include "lib/string.hpp"

namespace net {

bool ethernet_send(const uint8_t dest_mac[6], uint16_t ethertype, const uint8_t* payload, size_t payload_len) {
    if (!dest_mac || !payload || payload_len == 0 || payload_len > 1500) {
        return false;
    }

    uint8_t buffer[1514];
    auto* hdr = reinterpret_cast<EthernetHeader*>(buffer);

    lib::memcpy(hdr->dest_mac, dest_mac, 6);
    net_get_mac(hdr->src_mac);
    hdr->ethertype = htons(ethertype);

    lib::memcpy(buffer + sizeof(EthernetHeader), payload, payload_len);

    size_t total_len = sizeof(EthernetHeader) + payload_len;
    if (total_len < 60) {
        lib::memset(buffer + total_len, 0, 60 - total_len);
        total_len = 60;
    }

    return drivers::e1000_send_packet(buffer, total_len);
}

void ethernet_handle_packet(const uint8_t* frame, size_t len) {
    if (!frame || len < sizeof(EthernetHeader)) {
        return;
    }

    const auto* hdr = reinterpret_cast<const EthernetHeader*>(frame);
    uint16_t ethertype = ntohs(hdr->ethertype);

    const uint8_t* payload = frame + sizeof(EthernetHeader);
    size_t payload_len = len - sizeof(EthernetHeader);

    if (ethertype == ETHERTYPE_ARP) {
        arp_handle_packet(payload, payload_len);
    } else if (ethertype == ETHERTYPE_IPV4) {
        ipv4_handle_packet(payload, payload_len);
    }
}

}
