#ifndef __mixer_h_
#define __mixer_h_

struct mixer_input {
    void* data;
    size_t len;
    size_t consumed;
};

void mixer(struct mixer_input* inputs, size_t n,
           void* output_buf, size_t* output_size,
           unsigned long tx_interval_ms);

#endif
