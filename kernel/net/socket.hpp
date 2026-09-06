#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace net {

inline constexpr int AF_UNSPEC = 0;
inline constexpr int AF_UNIX   = 1;
inline constexpr int AF_INET   = 2;
inline constexpr int AF_INET6  = 10;

inline constexpr int SOCK_STREAM = 1;
inline constexpr int SOCK_DGRAM  = 2;
inline constexpr int SOCK_RAW    = 3;

inline constexpr int IPPROTO_IP   = 0;
inline constexpr int IPPROTO_ICMP = 1;
inline constexpr int IPPROTO_TCP  = 6;
inline constexpr int IPPROTO_UDP  = 17;

inline constexpr int SHUT_RD   = 0;
inline constexpr int SHUT_WR   = 1;
inline constexpr int SHUT_RDWR = 2;

inline constexpr int SOL_SOCKET = 1;

inline constexpr int SO_DEBUG       = 1;
inline constexpr int SO_REUSEADDR   = 2;
inline constexpr int SO_TYPE        = 3;
inline constexpr int SO_ERROR       = 4;
inline constexpr int SO_DONTROUTE   = 5;
inline constexpr int SO_BROADCAST   = 6;
inline constexpr int SO_SNDBUF      = 7;
inline constexpr int SO_RCVBUF      = 8;
inline constexpr int SO_KEEPALIVE   = 9;
inline constexpr int SO_OOBINLINE   = 10;
inline constexpr int SO_NO_CHECK    = 11;
inline constexpr int SO_PRIORITY    = 12;
inline constexpr int SO_LINGER      = 13;
inline constexpr int SO_BSDCOMPAT   = 14;
inline constexpr int SO_REUSEPORT   = 15;
inline constexpr int SO_RCVTIMEO    = 20;
inline constexpr int SO_SNDTIMEO    = 21;

inline constexpr short POLLIN   = 0x0001;
inline constexpr short POLLPRI  = 0x0002;
inline constexpr short POLLOUT  = 0x0004;
inline constexpr short POLLERR  = 0x0008;
inline constexpr short POLLHUP  = 0x0010;
inline constexpr short POLLNVAL = 0x0020;

struct in_addr {
    uint32_t s_addr;
};

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct sockaddr_in {
    uint16_t       sin_family;
    uint16_t       sin_port;
    struct in_addr sin_addr;
    uint8_t        sin_zero[8];
};

struct pollfd {
    int   fd;
    short events;
    short revents;
};

void    sock_init();
int     sock_create(int domain, int type, int protocol);
int     sock_connect(int fd, const sockaddr_in* addr);
int     sock_bind(int fd, const sockaddr_in* addr);
int     sock_listen(int fd, int backlog);
int     sock_accept(int fd, sockaddr_in* addr);
int64_t sock_send(int fd, const void* buf, size_t len, int flags);
int64_t sock_sendto(int fd, const void* buf, size_t len, int flags, const sockaddr_in* dest_addr);
int64_t sock_recv(int fd, void* buf, size_t len, int flags);
int64_t sock_recvfrom(int fd, void* buf, size_t len, int flags, sockaddr_in* src_addr);
int     sock_shutdown(int fd, int how);
int     sock_close(int fd);
int     sock_getsockname(int fd, sockaddr_in* addr);
int     sock_getpeername(int fd, sockaddr_in* addr);
int     sock_setsockopt(int fd, int level, int optname, const void* optval, uint32_t optlen);
int     sock_getsockopt(int fd, int level, int optname, void* optval, uint32_t* optlen);
bool    sock_is_socket(int fd);
bool    sock_poll_ready(int fd, bool check_write);

}
