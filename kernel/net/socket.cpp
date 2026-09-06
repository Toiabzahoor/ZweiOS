#include "net/socket.hpp"
#include "net/tcp.hpp"
#include "net/udp.hpp"
#include "net/ipv4.hpp"
#include "net/ethernet.hpp"
#include "net/net.hpp"
#include "lib/string.hpp"

namespace net {

inline constexpr size_t MAX_SOCKETS = 32;
inline constexpr int    FD_SOCKET_OFFSET = 100;

struct SocketEntry {
    bool        in_use;
    int         domain;
    int         type;
    int         protocol;
    int         tcp_sock_id;
    uint16_t    local_port;
    uint32_t    remote_ip;
    uint16_t    remote_port;
    bool        listening;
    bool        connected;
    uint8_t     rx_buf[4096];
    size_t      rx_head;
    size_t      rx_tail;
    size_t      rx_count;
};

static SocketEntry g_sockets[MAX_SOCKETS];
static uint16_t    g_next_ephemeral_port = 49152;

static void udp_socket_rx_callback(uint32_t src_ip, uint16_t src_port, const uint8_t* data, size_t len) {
    (void)src_ip;
    (void)src_port;
    if (!data || len == 0) return;

    for (size_t i = 0; i < MAX_SOCKETS; ++i) {
        if (g_sockets[i].in_use && g_sockets[i].type == SOCK_DGRAM) {
            size_t avail = sizeof(g_sockets[i].rx_buf) - g_sockets[i].rx_count;
            size_t to_copy = (len < avail) ? len : avail;
            for (size_t j = 0; j < to_copy; ++j) {
                g_sockets[i].rx_buf[g_sockets[i].rx_tail] = data[j];
                g_sockets[i].rx_tail = (g_sockets[i].rx_tail + 1) % sizeof(g_sockets[i].rx_buf);
            }
            g_sockets[i].rx_count += to_copy;
        }
    }
}

void sock_init() {
    for (size_t i = 0; i < MAX_SOCKETS; ++i) {
        g_sockets[i].in_use = false;
        g_sockets[i].tcp_sock_id = -1;
        g_sockets[i].local_port = 0;
        g_sockets[i].remote_ip = 0;
        g_sockets[i].remote_port = 0;
        g_sockets[i].listening = false;
        g_sockets[i].connected = false;
        g_sockets[i].rx_head = 0;
        g_sockets[i].rx_tail = 0;
        g_sockets[i].rx_count = 0;
    }
}

int sock_create(int domain, int type, int protocol) {
    if (domain != AF_INET && domain != AF_UNSPEC) {
        return -1;
    }

    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        return -1;
    }

    for (size_t i = 0; i < MAX_SOCKETS; ++i) {
        if (!g_sockets[i].in_use) {
            g_sockets[i].in_use = true;
            g_sockets[i].domain = domain;
            g_sockets[i].type = type;
            g_sockets[i].protocol = protocol;
            g_sockets[i].tcp_sock_id = -1;
            g_sockets[i].local_port = g_next_ephemeral_port++;
            if (g_next_ephemeral_port < 49152) g_next_ephemeral_port = 49152;
            g_sockets[i].remote_ip = 0;
            g_sockets[i].remote_port = 0;
            g_sockets[i].listening = false;
            g_sockets[i].connected = false;
            g_sockets[i].rx_head = 0;
            g_sockets[i].rx_tail = 0;
            g_sockets[i].rx_count = 0;

            if (type == SOCK_STREAM) {
                g_sockets[i].tcp_sock_id = tcp_socket_create();
            } else if (type == SOCK_DGRAM) {
                udp_bind(g_sockets[i].local_port, udp_socket_rx_callback);
            }

            return static_cast<int>(FD_SOCKET_OFFSET + i);
        }
    }

    return -1;
}

int sock_connect(int fd, const sockaddr_in* addr) {
    if (!sock_is_socket(fd) || !addr) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    s.remote_ip = addr->sin_addr.s_addr;
    s.remote_port = ntohs(addr->sin_port);

    if (s.type == SOCK_STREAM) {
        if (s.tcp_sock_id < 0) {
            s.tcp_sock_id = tcp_socket_create();
            if (s.tcp_sock_id < 0) return -1;
        }
        bool ok = tcp_connect(s.tcp_sock_id, s.remote_ip, s.remote_port);
        if (ok) {
            s.connected = true;
            return 0;
        }
        return -1;
    } else if (s.type == SOCK_DGRAM) {
        s.connected = true;
        return 0;
    }

    return -1;
}

int sock_bind(int fd, const sockaddr_in* addr) {
    if (!sock_is_socket(fd) || !addr) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    s.local_port = ntohs(addr->sin_port);
    if (s.type == SOCK_DGRAM) {
        udp_bind(s.local_port, udp_socket_rx_callback);
    }
    return 0;
}

int sock_listen(int fd, int backlog) {
    (void)backlog;
    if (!sock_is_socket(fd)) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];
    if (s.type != SOCK_STREAM) return -1;
    s.listening = true;
    return 0;
}

int sock_accept(int fd, sockaddr_in* addr) {
    if (!sock_is_socket(fd)) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];
    if (!s.listening) return -1;

    int new_fd = sock_create(s.domain, s.type, s.protocol);
    if (new_fd < 0) return -1;

    if (addr) {
        addr->sin_family = AF_INET;
        addr->sin_port = htons(s.local_port);
        addr->sin_addr.s_addr = 0x0100007FU;
        lib::memset(addr->sin_zero, 0, 8);
    }

    return new_fd;
}

int64_t sock_send(int fd, const void* buf, size_t len, int flags) {
    (void)flags;
    if (!sock_is_socket(fd) || !buf || len == 0) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    if (s.type == SOCK_STREAM) {
        if (s.tcp_sock_id < 0) return -1;
        int sent = tcp_send(s.tcp_sock_id, reinterpret_cast<const uint8_t*>(buf), len);
        if (sent < 0) return -1;
        return static_cast<int64_t>(sent);
    } else if (s.type == SOCK_DGRAM) {
        if (s.remote_ip == 0 || s.remote_port == 0) return -1;
        bool ok = udp_send(s.remote_ip, s.local_port, s.remote_port, reinterpret_cast<const uint8_t*>(buf), len);
        return ok ? static_cast<int64_t>(len) : -1;
    }

    return -1;
}

int64_t sock_sendto(int fd, const void* buf, size_t len, int flags, const sockaddr_in* dest_addr) {
    if (!dest_addr) {
        return sock_send(fd, buf, len, flags);
    }
    if (!sock_is_socket(fd) || !buf || len == 0) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    uint32_t dst_ip = dest_addr->sin_addr.s_addr;
    uint16_t dst_port = ntohs(dest_addr->sin_port);

    if (s.type == SOCK_DGRAM) {
        bool ok = udp_send(dst_ip, s.local_port, dst_port, reinterpret_cast<const uint8_t*>(buf), len);
        return ok ? static_cast<int64_t>(len) : -1;
    } else {
        return sock_send(fd, buf, len, flags);
    }
}

int64_t sock_recv(int fd, void* buf, size_t len, int flags) {
    (void)flags;
    if (!sock_is_socket(fd) || !buf || len == 0) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    if (s.type == SOCK_STREAM) {
        if (s.tcp_sock_id < 0) return -1;
        int received = tcp_recv(s.tcp_sock_id, reinterpret_cast<uint8_t*>(buf), len);
        if (received < 0) return -1;
        return static_cast<int64_t>(received);
    } else if (s.type == SOCK_DGRAM) {
        if (s.rx_count == 0) {
            net_poll();
        }
        if (s.rx_count == 0) return 0;

        size_t to_read = (len < s.rx_count) ? len : s.rx_count;
        auto* out_bytes = reinterpret_cast<uint8_t*>(buf);
        for (size_t i = 0; i < to_read; ++i) {
            out_bytes[i] = s.rx_buf[s.rx_head];
            s.rx_head = (s.rx_head + 1) % sizeof(s.rx_buf);
        }
        s.rx_count -= to_read;
        return static_cast<int64_t>(to_read);
    }

    return -1;
}

int64_t sock_recvfrom(int fd, void* buf, size_t len, int flags, sockaddr_in* src_addr) {
    int64_t ret = sock_recv(fd, buf, len, flags);
    if (ret >= 0 && src_addr && sock_is_socket(fd)) {
        int idx = fd - FD_SOCKET_OFFSET;
        src_addr->sin_family = AF_INET;
        src_addr->sin_port = htons(g_sockets[idx].remote_port);
        src_addr->sin_addr.s_addr = g_sockets[idx].remote_ip;
        lib::memset(src_addr->sin_zero, 0, 8);
    }
    return ret;
}

int sock_shutdown(int fd, int how) {
    (void)how;
    if (!sock_is_socket(fd)) return -1;
    return 0;
}

int sock_close(int fd) {
    if (!sock_is_socket(fd)) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    if (s.type == SOCK_STREAM && s.tcp_sock_id >= 0) {
        tcp_close(s.tcp_sock_id);
        s.tcp_sock_id = -1;
    } else if (s.type == SOCK_DGRAM && s.local_port > 0) {
        udp_unbind(s.local_port);
    }

    s.in_use = false;
    s.connected = false;
    s.listening = false;
    return 0;
}

int sock_getsockname(int fd, sockaddr_in* addr) {
    if (!sock_is_socket(fd) || !addr) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(g_sockets[idx].local_port);
    InterfaceConfig cfg = net_get_config();
    addr->sin_addr.s_addr = cfg.ip;
    lib::memset(addr->sin_zero, 0, 8);
    return 0;
}

int sock_getpeername(int fd, sockaddr_in* addr) {
    if (!sock_is_socket(fd) || !addr) return -1;
    int idx = fd - FD_SOCKET_OFFSET;
    if (!g_sockets[idx].connected) return -1;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(g_sockets[idx].remote_port);
    addr->sin_addr.s_addr = g_sockets[idx].remote_ip;
    lib::memset(addr->sin_zero, 0, 8);
    return 0;
}

int sock_setsockopt(int fd, int level, int optname, const void* optval, uint32_t optlen) {
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    if (!sock_is_socket(fd)) return -1;
    return 0;
}

int sock_getsockopt(int fd, int level, int optname, void* optval, uint32_t* optlen) {
    (void)level;
    (void)optname;
    if (!sock_is_socket(fd) || !optval || !optlen) return -1;
    if (*optlen >= sizeof(int)) {
        *reinterpret_cast<int*>(optval) = 0;
        *optlen = sizeof(int);
    }
    return 0;
}

bool sock_is_socket(int fd) {
    if (fd < FD_SOCKET_OFFSET || fd >= static_cast<int>(FD_SOCKET_OFFSET + MAX_SOCKETS)) {
        return false;
    }
    return g_sockets[fd - FD_SOCKET_OFFSET].in_use;
}

bool sock_poll_ready(int fd, bool check_write) {
    if (!sock_is_socket(fd)) return false;
    int idx = fd - FD_SOCKET_OFFSET;
    auto& s = g_sockets[idx];

    if (check_write) {
        return s.connected;
    } else {
        net_poll();
        if (s.type == SOCK_STREAM && s.tcp_sock_id >= 0) {
            TCPEndpoint ep;
            if (tcp_get_endpoint_info(static_cast<size_t>(s.tcp_sock_id), &ep)) {
                return ep.rx_len > 0;
            }
        } else if (s.type == SOCK_DGRAM) {
            return s.rx_count > 0;
        }
    }
    return false;
}

}
