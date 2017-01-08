#include <iostream>
#include <exception>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <signal.h>
#include "client_params.h"
#include "client.h"


using std::cout;
using std::endl;


#ifdef NDEBUG
    const bool DEBUG = false;
#else
    const bool DEBUG = true;
#endif

void run_client(ClientParams params) {
while (true) {
    std::cerr << "Runclient..." << endl;
    boost::asio::io_service io_service;
    Client client(params, io_service);
    io_service.run();
    std::cerr << "Terminating...\n";
    boost::this_thread::sleep(boost::posix_time::milliseconds(800)); 
}
}

int main(int argc, char** argv) {

try {
    namespace po = boost::program_options;
    
    ClientParams params;
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "")
        ("server_name,s", po::value<std::string>(&params.server_name)->required())
        ("port,p", po::value<uint16_t>(&params.port)->default_value(DEFAULT_PORT))
        ("retransmit_limit,X", po::value<size_t>(&params.retransmit_limit)->default_value(DEFAULT_RETRANSMIT_LIMIT))
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (DEBUG) {
        cout << "Settings:" << endl;
        cout << "server_name         -- " << params.server_name << endl;
        cout << "port                -- " << params.port << endl;
        cout << "retransmit_limit    -- " << params.retransmit_limit << endl;
    }

    if (vm.count("help")) {
        cout << "E-ognisko -- client" << endl << desc << endl;
    } else {
        run_client(params);
    }
} catch (std::exception &e) {
    cout << e.what() << endl;
}

    return 0;
}
