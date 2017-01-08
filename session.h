#ifndef __session_h_
#define __session_h_

#include <cstdint>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include "server_params.h"

using std::shared_ptr;
using std::string;
using std::vector;
using std::pair;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

enum fifo_state_t { FILLING, ACTIVE };

class Session {
public:
    Session(uint32_t, ServerParams&, shared_ptr<tcp::socket>);
    
    string get_info();
    string get_datagram_header(uint32_t);
    string get_ack_header();
    string get_client_header();
    
    pair<void*, size_t> get_data();
    void consume(size_t);
    void reset_fifo_stats();
    void reset_alive_stats();
    void init_udp(udp::endpoint);
    void keepalive();
    void upload(string);
    size_t get_win();
    

    /* Identification */
    uint32_t id;

    /* Parameters */
    ServerParams & params;

    /* TCP connection */
    shared_ptr<tcp::socket> tcp_socket_p;
    tcp::endpoint tcp_remote_endpoint;

    /* UDP connection */
    bool uses_udp;
    udp::endpoint udp_remote_endpoint;
    uint32_t ack;

    /* FIFO */
    vector<char> fifo;
    fifo_state_t fifo_state;

    /* REPORT STATISTICS */
    size_t fifo_max;
    size_t fifo_min;
    
    /* KEEPALIVE STATISTICS */
    bool udp_alive;
};

#endif
