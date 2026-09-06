#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

inline constexpr uint8_t TCP_FLAG_FIN = (1U << 0);
inline constexpr uint8_t TCP_FLAG_SYN = (1U << 1);
inline constexpr uint8_t TCP_FLAG_RST = (1U << 2);
inline constexpr uint8_t TCP_FLAG_PSH = (1U << 3);
inline constexpr uint8_t TCP_FLAG_ACK = (1U << 4);
inline constexpr uint8_t TCP_FLAG_URG = (1U << 5);

enum class TCPState {
    CLOSED = 0,
    LISTEN,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    LAST_ACK,
    TIME_WAIT
};

struct TCPHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

struct TCPEndpoint {
    int       id;
    bool      in_use;
    TCPState  state;
    uint32_t  local_ip;
    uint16_t  local_port;
    uint32_t  remote_ip;
    uint16_t  remote_port;
    uint32_t  seq;
    uint32_t  ack;
    uint8_t   rx_buffer[4096];
    size_t    rx_len;
};

void     tcp_init();
int      tcp_socket_create();
bool     tcp_connect(int sock_id, uint32_t dst_ip, uint16_t dst_port);
int      tcp_send(int sock_id, const uint8_t* data, size_t len);
int      tcp_recv(int sock_id, uint8_t* out_buf, size_t max_len);
void     tcp_close(int sock_id);
void     tcp_handle_packet(uint32_t src_ip, const uint8_t* payload, size_t len);

size_t   tcp_get_endpoint_count();
bool     tcp_get_endpoint_info(size_t index, TCPEndpoint* out_info);

}
