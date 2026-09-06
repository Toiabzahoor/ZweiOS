#include "net/tcp.hpp"
#include "net/ipv4.hpp"
#include "net/ethernet.hpp"
#include "net/net.hpp"
#include "drivers/pit.hpp"
#include "lib/string.hpp"

namespace net {

static constexpr size_t MAX_TCP_SOCKETS = 16;
static TCPEndpoint g_tcp_sockets[MAX_TCP_SOCKETS];
static uint16_t    g_next_local_port = 49152;
static uint32_t    g_initial_seq = 100000;

struct TCPPseudoHeader {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed));

void tcp_init() {
    for (size_t i = 0; i < MAX_TCP_SOCKETS; ++i) {
        g_tcp_sockets[i].id = static_cast<int>(i);
        g_tcp_sockets[i].in_use = false;
        g_tcp_sockets[i].state = TCPState::CLOSED;
        g_tcp_sockets[i].local_ip = 0;
        g_tcp_sockets[i].local_port = 0;
        g_tcp_sockets[i].remote_ip = 0;
        g_tcp_sockets[i].remote_port = 0;
        g_tcp_sockets[i].seq = 0;
        g_tcp_sockets[i].ack = 0;
        g_tcp_sockets[i].rx_len = 0;
    }
}

static bool tcp_send_segment(TCPEndpoint* sock, uint8_t flags, const uint8_t* payload, size_t payload_len) {
    if (!sock) return false;

    uint8_t packet[1500];
    auto* hdr = reinterpret_cast<TCPHeader*>(packet);

    uint16_t tcp_total_len = static_cast<uint16_t>(sizeof(TCPHeader) + payload_len);
    hdr->src_port    = htons(sock->local_port);
    hdr->dst_port    = htons(sock->remote_port);
    hdr->seq_num     = htonl(sock->seq);
    hdr->ack_num     = htonl(sock->ack);
    hdr->data_offset = static_cast<uint8_t>((sizeof(TCPHeader) / 4) << 4);
    hdr->flags       = flags;
    hdr->window_size = htons(4096);
    hdr->checksum    = 0;
    hdr->urgent_ptr  = 0;

    if (payload && payload_len > 0) {
        lib::memcpy(packet + sizeof(TCPHeader), payload, payload_len);
    }

    uint8_t pseudo_buf[1500];
    auto* phdr = reinterpret_cast<TCPPseudoHeader*>(pseudo_buf);
    phdr->src_ip = sock->local_ip;
    phdr->dst_ip = sock->remote_ip;
    phdr->zero = 0;
    phdr->protocol = IP_PROTO_TCP;
    phdr->tcp_len = htons(tcp_total_len);

    lib::memcpy(pseudo_buf + sizeof(TCPPseudoHeader), packet, tcp_total_len);
    hdr->checksum = ipv4_checksum(pseudo_buf, sizeof(TCPPseudoHeader) + tcp_total_len);
    if (hdr->checksum == 0) hdr->checksum = 0xFFFF;

    return ipv4_send(sock->remote_ip, IP_PROTO_TCP, packet, tcp_total_len);
}

int tcp_socket_create() {
    for (size_t i = 0; i < MAX_TCP_SOCKETS; ++i) {
        if (!g_tcp_sockets[i].in_use) {
            g_tcp_sockets[i].in_use = true;
            g_tcp_sockets[i].state = TCPState::CLOSED;
            g_tcp_sockets[i].local_ip = net_get_config().ip;
            g_tcp_sockets[i].local_port = g_next_local_port++;
            if (g_next_local_port >= 65000) g_next_local_port = 49152;
            g_tcp_sockets[i].remote_ip = 0;
            g_tcp_sockets[i].remote_port = 0;
            g_tcp_sockets[i].seq = g_initial_seq += 10000;
            g_tcp_sockets[i].ack = 0;
            g_tcp_sockets[i].rx_len = 0;
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool tcp_connect(int sock_id, uint32_t dst_ip, uint16_t dst_port) {
    if (sock_id < 0 || sock_id >= static_cast<int>(MAX_TCP_SOCKETS) || !g_tcp_sockets[sock_id].in_use) {
        return false;
    }

    TCPEndpoint* sock = &g_tcp_sockets[sock_id];
    sock->local_ip = net_get_config().ip;
    sock->remote_ip = dst_ip;
    sock->remote_port = dst_port;
    sock->state = TCPState::SYN_SENT;

    if (!tcp_send_segment(sock, TCP_FLAG_SYN, nullptr, 0)) {
        sock->state = TCPState::CLOSED;
        return false;
    }

    uint64_t start_ticks = drivers::pit_get_ticks();
    for (uint32_t i = 0; i < 20000 && sock->state != TCPState::ESTABLISHED; ++i) {
        net_poll();
        if (drivers::pit_get_ticks() - start_ticks > 300) {
            sock->state = TCPState::CLOSED;
            return false;
        }
    }

    return true;
}

int tcp_send(int sock_id, const uint8_t* data, size_t len) {
    if (sock_id < 0 || sock_id >= static_cast<int>(MAX_TCP_SOCKETS) || !g_tcp_sockets[sock_id].in_use) {
        return -1;
    }

    TCPEndpoint* sock = &g_tcp_sockets[sock_id];
    if (sock->state != TCPState::ESTABLISHED || !data || len == 0) {
        return -1;
    }

    size_t chunk_len = (len > 1400) ? 1400 : len;
    if (!tcp_send_segment(sock, TCP_FLAG_ACK | TCP_FLAG_PSH, data, chunk_len)) {
        return -1;
    }

    sock->seq += static_cast<uint32_t>(chunk_len);
    return static_cast<int>(chunk_len);
}

int tcp_recv(int sock_id, uint8_t* out_buf, size_t max_len) {
    if (sock_id < 0 || sock_id >= static_cast<int>(MAX_TCP_SOCKETS) || !g_tcp_sockets[sock_id].in_use) {
        return -1;
    }

    TCPEndpoint* sock = &g_tcp_sockets[sock_id];
    if (sock->rx_len == 0) {
        return 0;
    }

    size_t copy_len = (sock->rx_len < max_len) ? sock->rx_len : max_len;
    lib::memcpy(out_buf, sock->rx_buffer, copy_len);

    if (copy_len < sock->rx_len) {
        size_t remaining = sock->rx_len - copy_len;
        lib::memcpy(sock->rx_buffer, sock->rx_buffer + copy_len, remaining);
        sock->rx_len = remaining;
    } else {
        sock->rx_len = 0;
    }

    return static_cast<int>(copy_len);
}

void tcp_close(int sock_id) {
    if (sock_id < 0 || sock_id >= static_cast<int>(MAX_TCP_SOCKETS) || !g_tcp_sockets[sock_id].in_use) {
        return;
    }

    TCPEndpoint* sock = &g_tcp_sockets[sock_id];
    if (sock->state == TCPState::ESTABLISHED) {
        tcp_send_segment(sock, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0);
        sock->state = TCPState::FIN_WAIT_1;
    } else {
        sock->state = TCPState::CLOSED;
        sock->in_use = false;
    }
}

void tcp_handle_packet(uint32_t src_ip, const uint8_t* payload, size_t len) {
    if (!payload || len < sizeof(TCPHeader)) {
        return;
    }

    const auto* hdr = reinterpret_cast<const TCPHeader*>(payload);
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint32_t seq_in   = ntohl(hdr->seq_num);
    uint32_t ack_in   = ntohl(hdr->ack_num);
    uint8_t  flags    = hdr->flags;

    size_t header_len = ((hdr->data_offset >> 4) & 0x0F) * 4;
    if (len < header_len) return;

    const uint8_t* data = payload + header_len;
    size_t data_len = len - header_len;

    for (size_t i = 0; i < MAX_TCP_SOCKETS; ++i) {
        TCPEndpoint* sock = &g_tcp_sockets[i];
        if (!sock->in_use) continue;

        if (sock->local_port == dst_port && (sock->remote_port == 0 || sock->remote_port == src_port)) {
            if (sock->state == TCPState::SYN_SENT) {
                if ((flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK)) {
                    sock->ack = seq_in + 1;
                    sock->seq = ack_in;
                    sock->state = TCPState::ESTABLISHED;
                    tcp_send_segment(sock, TCP_FLAG_ACK, nullptr, 0);
                    return;
                }
            } else if (sock->state == TCPState::ESTABLISHED) {
                if (flags & TCP_FLAG_RST) {
                    sock->state = TCPState::CLOSED;
                    sock->in_use = false;
                    return;
                }

                if (data_len > 0) {
                    if (sock->rx_len + data_len <= sizeof(sock->rx_buffer)) {
                        lib::memcpy(sock->rx_buffer + sock->rx_len, data, data_len);
                        sock->rx_len += data_len;
                    }
                    sock->ack = seq_in + static_cast<uint32_t>(data_len);
                    tcp_send_segment(sock, TCP_FLAG_ACK, nullptr, 0);
                }

                if (flags & TCP_FLAG_FIN) {
                    sock->ack = seq_in + 1;
                    tcp_send_segment(sock, TCP_FLAG_ACK, nullptr, 0);
                    sock->state = TCPState::CLOSE_WAIT;
                    tcp_send_segment(sock, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0);
                    sock->state = TCPState::CLOSED;
                    sock->in_use = false;
                }
                return;
            } else if (sock->state == TCPState::FIN_WAIT_1) {
                if (flags & TCP_FLAG_ACK) {
                    sock->state = TCPState::CLOSED;
                    sock->in_use = false;
                }
                return;
            }
        }
    }
}

size_t tcp_get_endpoint_count() {
    return MAX_TCP_SOCKETS;
}

bool tcp_get_endpoint_info(size_t index, TCPEndpoint* out_info) {
    if (!out_info || index >= MAX_TCP_SOCKETS) return false;
    *out_info = g_tcp_sockets[index];
    return true;
}

}
