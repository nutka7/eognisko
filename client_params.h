#ifndef __client_params_h_
#define __client_params_h_

#include <stdint.h>
#include <cstddef>
#include <string>

const uint16_t DEFAULT_PORT = (10000 + 337620) % 10000;
const size_t DEFAULT_RETRANSMIT_LIMIT = 10;

typedef struct {
    std::string server_name;
    uint16_t port;
    size_t retransmit_limit;
} ClientParams;

#endif
