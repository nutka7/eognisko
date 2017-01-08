#include <iostream>
#include <exception>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <signal.h>
#include "server_params.h"
#include "server.h"

#ifdef NDEBUG
    const bool DEBUG = false;
#else
    const bool DEBUG = true;
#endif


using std::cout;
using std::endl;


boost::asio::io_service io_service;

void on_signal(int signum) {
        io_service.stop();
}



void setup_signals() {
    struct sigaction action;
    action.sa_handler = on_signal;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}



void run_server(ServerParams params) {
    if (DEBUG) {
        cout << "Runserver..." << endl;
    }
    Server server(params, io_service);
    io_service.run();
}


int main(int argc, char **argv) {
    setup_signals();

try {
    ServerParams params;
    
    namespace po = boost::program_options;
    
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "")
        ("port,p", po::value<uint16_t>(&params.port)->default_value(DEFAULT_PORT))
        ("fifo_size,F", po::value<size_t>(&params.fifo_size)->default_value(DEFAULT_FIFO_SIZE))
        ("fifo_low_watermark,L", po::value<size_t>(&params.fifo_low_watermark)->default_value(DEFAULT_FIFO_LOW_WATERMARK))
        ("fifo_high_watermark,H", po::value<size_t>(&params.fifo_high_watermark))
        ("buf_len,X", po::value<size_t>(&params.buf_len)->default_value(DEFAULT_BUF_LEN))
        ("tx_interval,i", po::value<unsigned long>(&params.tx_interval)->default_value(DEFAULT_TX_INTERVAL))
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (!vm.count("fifo_high_watermark")) {
        params.fifo_high_watermark = params.fifo_size;
    }

    if (DEBUG) {    
        cout << "Settings:" << endl;
        cout << "port                -- " << params.port <<endl;
        cout << "fifo_size           -- " << params.fifo_size << endl;
        cout << "fifo_low_watermark  -- " << params.fifo_low_watermark << endl;
        cout << "fifo_high_watermark -- " << params.fifo_high_watermark << endl;
        cout << "buf_len             -- " << params.buf_len << endl;
        cout << "tx_interval         -- " << params.tx_interval << endl;
    }

    if (vm.count("help")) {
        cout << "E-ognisko -- server" << endl << desc << endl;
    } else {
        run_server(params);
    }
} catch (std::exception & e) {
    cout << e.what() << endl;
}
   
    return 0;
}
