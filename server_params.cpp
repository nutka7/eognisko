#include "server_params.h"

#include <iostream>
#include <string>
#include <sstream>
#include "boost/program_options.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/bind.hpp"


ServerParams::DEFAULT_PORT = (10000 + ALBUM_NR) % 10000;
ServerParams::DEFAULT_FIFO_SIZE = 10560;
ServerParams::DEFAULT_FIFO_LOW_WATERMARK = 0;
ServerParams::DEFAULT_BUF_LEN = 5;
ServerParams::DEFAULT_TX_INTERVAL = 10; 


ServerParams::ServerParams(int argc, char **argv) {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
        (",p", po::value<uint16_t>(&_port)->default_value(DEFAULT_PORT),
         "port number")
        (",F", po::value<int>(&_fifo_size)->default_value(DEFAULT_FIFO_SIZE),
         "fifo size")
        (",L", po::value<int>(&_fifo_low_watermark)->default_value(DEFAULT_FIFO_LOW_WATERMARK),
         "fifo low watermark")
        (",H", po::value<int>(&_fifo_high_watermark),
         "fifo_high_watermark")
        (",X", po::value<size_t>(&_buf_len)->default_value(DEFAULT_BUF_LEN),
         "buffer length")
        (",i", po::value<int>(&_tx_interval)->default_value(DEFAULT_TX_INTERVAL),
         "tx interval")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (!vm.count("fifo_high_watermark")) {
        _fifo_high_watermark = _fifo_size;
    }
    std::cout << desc << std::endl;
}

int main(int argc, char **argv) {
    ServerParams(argc, argv);
    return 0;
}

