#ifndef __server_params_h_
#define __server_params_h_
#include <stdint.h>
#include <cstddef>

const int ALBUM_NR = 337620;

const uint16_t DEFAULT_PORT = (10000 + ALBUM_NR) % 10000;
const size_t DEFAULT_FIFO_SIZE = 10560;
const size_t DEFAULT_FIFO_LOW_WATERMARK = 0;
const size_t DEFAULT_BUF_LEN = 10;
const unsigned long DEFAULT_TX_INTERVAL = 5;

typedef struct {
        uint16_t port;
        size_t fifo_size;
        size_t fifo_low_watermark;
        size_t fifo_high_watermark;
        size_t buf_len;
        unsigned long tx_interval;
} ServerParams;

#endif
