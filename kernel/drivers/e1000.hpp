#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace drivers {

struct E1000RxDesc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

struct E1000TxDesc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

bool   e1000_init();
bool   e1000_is_available();
void   e1000_get_mac(uint8_t out_mac[6]);
bool   e1000_send_packet(const uint8_t* data, size_t len);
size_t e1000_receive_packet(uint8_t* out_buf, size_t max_len);
void   e1000_handle_irq();

uint64_t e1000_get_rx_packets();
uint64_t e1000_get_tx_packets();
uint64_t e1000_get_rx_bytes();
uint64_t e1000_get_tx_bytes();

}
