#ifndef __server_h_
#define __server_h_

#include <map>
#include <string>
#include <boost/asio.hpp>
#include "server_params.h"
#include "session.h"
#include "mixer.h"

using std::shared_ptr;
using std::string;
using std::map;
using std::pair;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

const size_t OUTPUT_BUF_SIZE = 10000;
const size_t RECV_BUF_LEN = 100000;

class Server
{
public:
    Server(ServerParams, boost::asio::io_service &);

    /* REPORTS */
    void schedule_report();
    void report(const boost::system::error_code&);
    string construct_report();
    void multi_send_report(shared_ptr<string>);
    void send_report(shared_ptr<Session>, shared_ptr<string>);
    void handle_send_report(const boost::system::error_code&, size_t, shared_ptr<Session>, shared_ptr<string>);
    
    /* BAD SESSIONS REMOVAL */
    void remove_session(shared_ptr<Session>);
    void schedule_remove_bad_sessions();
    void remove_bad_sessions(const boost::system::error_code& ec);

    /* MIX AND SEND */
    void schedule_mix_and_send();
    void mix_and_send(const boost::system::error_code& ec);
    
    string mix();
    vector<mixer_input> construct_mixer_inputs();
    void multi_consume(vector<mixer_input> & inputs);
    void multi_reset_fifo_stats();

    void multi_send_remix_datagram(uint32_t);
    void send_remix_datagram(shared_ptr<Session>, uint32_t);
    void handle_send_remix_datagram(const boost::system::error_code&, size_t, shared_ptr<Session>, shared_ptr<string>);
    string construct_remix_datagram(shared_ptr<Session>, uint32_t);

    /* ACCEPTING TCP */
    void accept_tcp();
    void handle_accept_tcp(const boost::system::error_code&, shared_ptr<tcp::socket>);
    void send_client_message(shared_ptr<Session>); /* Tu jest hak, bo używam send_report */

    /* ACCEPTING UDP */
    void receive_udp();
    void handle_receive_udp(const boost::system::error_code&, size_t);

    void client(udp::endpoint, uint32_t);
    void upload(udp::endpoint, string, uint32_t);
    void retransmit(udp::endpoint, uint32_t);
    void keepalive(udp::endpoint);
    void send_ack(shared_ptr<Session>);
    void handle_send_ack(const boost::system::error_code&, size_t n, shared_ptr<Session>, shared_ptr<string>);



private:

    /* --- DATA --- */

    ServerParams params;
    boost::asio::io_service & io_service;
    
    /* TCP */
    tcp::acceptor acceptor;
    
    /* UDP */
    udp::socket udp_socket;
    udp::endpoint udp_remote_endpoint;
    char recv_buf[RECV_BUF_LEN];

    /* TIMERS */
    boost::asio::deadline_timer report_timer;
    boost::asio::deadline_timer remove_bad_sessions_timer;
    boost::asio::deadline_timer mix_and_send_timer;

    /* MIXER */
    char output_buf[OUTPUT_BUF_SIZE];

    /* SESSIONS */
    map<uint32_t, shared_ptr<Session>> sessions;
    map<udp::endpoint, shared_ptr<Session>> endpoint_to_session; 
    uint32_t next_free_id;

    /* SENT DATAGRAMS */
    uint32_t remix_nr; // can be set to any number at start
    std::map<uint32_t, string> remixes;
};

#endif
