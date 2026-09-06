#include "net/icmp.hpp"
#include "net/ipv4.hpp"
#include "net/ethernet.hpp"
#include "net/net.hpp"
#include "drivers/pit.hpp"
#include "lib/string.hpp"

namespace net {

static volatile bool     g_ping_received = false;
static volatile uint16_t g_ping_match_id = 0;
static volatile uint16_t g_ping_match_seq = 0;

void icmp_init() {
    g_ping_received = false;
    g_ping_match_id = 0;
    g_ping_match_seq = 0;
}

void icmp_handle_packet(uint32_t src_ip, const uint8_t* payload, size_t len) {
    if (!payload || len < sizeof(ICMPHeader)) {
        return;
    }

    const auto* req = reinterpret_cast<const ICMPHeader*>(payload);

    if (req->type == ICMP_TYPE_ECHO_REQUEST) {
        uint8_t reply_buf[1500];
        if (len > sizeof(reply_buf)) return;

        lib::memcpy(reply_buf, payload, len);
        auto* rep = reinterpret_cast<ICMPHeader*>(reply_buf);
        rep->type = ICMP_TYPE_ECHO_REPLY;
        rep->checksum = 0;
        rep->checksum = ipv4_checksum(rep, len);

        ipv4_send(src_ip, IP_PROTO_ICMP, reply_buf, len);
    } else if (req->type == ICMP_TYPE_ECHO_REPLY) {
        if (ntohs(req->id) == g_ping_match_id && ntohs(req->seq) == g_ping_match_seq) {
            g_ping_received = true;
        }
    }
}

bool icmp_send_echo_request(uint32_t dst_ip, uint16_t id, uint16_t seq, const uint8_t* data, size_t data_len) {
    uint8_t buffer[128];
    size_t total_len = sizeof(ICMPHeader) + data_len;
    if (total_len > sizeof(buffer)) return false;

    auto* hdr = reinterpret_cast<ICMPHeader*>(buffer);
    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->id = htons(id);
    hdr->seq = htons(seq);
    hdr->checksum = 0;

    if (data && data_len > 0) {
        lib::memcpy(buffer + sizeof(ICMPHeader), data, data_len);
    }

    hdr->checksum = ipv4_checksum(buffer, total_len);

    return ipv4_send(dst_ip, IP_PROTO_ICMP, buffer, total_len);
}

bool icmp_ping(uint32_t dst_ip, uint32_t timeout_ms, uint32_t* out_rtt_ms) {
    static uint16_t seq_counter = 1;
    uint16_t id = 0x4321;
    uint16_t seq = seq_counter++;

    g_ping_received = false;
    g_ping_match_id = id;
    g_ping_match_seq = seq;

    const char ping_data[] = "ZweiOS ICMP Ping Probe";
    size_t dlen = lib::strlen(ping_data);

    uint64_t start_ticks = drivers::pit_get_ticks();

    if (!icmp_send_echo_request(dst_ip, id, seq, reinterpret_cast<const uint8_t*>(ping_data), dlen)) {
        return false;
    }

    uint64_t max_ticks = (static_cast<uint64_t>(timeout_ms) * 100ULL) / 1000ULL;
    if (max_ticks == 0) max_ticks = 1;

    for (uint32_t i = 0; i < 20000 && !g_ping_received; ++i) {
        net_poll();
        uint64_t elapsed_ticks = drivers::pit_get_ticks() - start_ticks;
        if (elapsed_ticks > max_ticks) {
            return false;
        }
    }

    uint64_t end_ticks = drivers::pit_get_ticks();
    uint64_t rtt = ((end_ticks - start_ticks) * 1000ULL) / 100ULL;
    if (rtt == 0) rtt = 1;

    if (out_rtt_ms) {
        *out_rtt_ms = static_cast<uint32_t>(rtt);
    }

    return true;
}

}
