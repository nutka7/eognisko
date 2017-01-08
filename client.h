#ifndef __client_h_
#define __client_h_

#include <map>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include "client_params.h"

using std::shared_ptr;
using std::string;
using std::map;
using std::pair;
using std::vector;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;


class Client
{
public:
    Client(ClientParams, boost::asio::io_service &);
    void terminate();
    
    /* RECEIVING ID AND REPORTS */
    void establish_tcp_connection();
    void receive_tcp(bool initial=false);
    void handle_receive_tcp(const boost::system::error_code &, size_t, bool initial=false);
    void set_id_from_msg(string);
    
    /* SENDING DATA AND ID */
    void setup_udp();
    void send_datagram(string, bool set_waiting=false);
    void handle_send_datagram(const boost::system::error_code &, size_t, shared_ptr<string>, bool);
    void send_id();
    void upload_data();
    void ask_for_retransmit(uint32_t nr_expected);
    void retransmit();
    
    /* RECEIVING DATA AND ACKS */
    void receive_udp();
    void handle_receive_udp(const boost::system::error_code&, size_t);
    void handle_ack(uint32_t ack, uint32_t _win, bool from_DATA=false);
    void handle_data_received(uint32_t nr, uint32_t ack, uint32_t win, string data);

    /* SENDING KEEPALIVE */
    void schedule_keepalive();
    void keepalive(const boost::system::error_code &);
    void handle_keepalive(const boost::system::error_code&, size_t, shared_ptr<string>);

    /* READING FROM STDIN */
    void read_stdin();
    void handle_read_stdin(const boost::system::error_code &, size_t n);

    /* CHECKING UDP CONNECTION */
    void schedule_check_udp_active();
    void check_udp_active(const boost::system::error_code&);



    /* --- DATA --- */

    ClientParams params;
    boost::asio::io_service & io_service;
    
    /* IDENTIFICATION */
    uint32_t id;

    /* TCP */
    boost::asio::ip::tcp::socket tcp_socket;
    char tcp_rcv_buf[70000];
    
    /* UDP */
    boost::asio::ip::udp::socket udp_socket;
    char udp_rcv_buf[70000];

    
    /* TIMERS */
    boost::asio::deadline_timer keepalive_timer;
    boost::asio::deadline_timer check_udp_timer;

    /* DATA RECEIVED INFO */
    uint32_t nr_max_seen;
    uint32_t nr_expected;

    /* SENDING UDP DATA */
    uint32_t next_ack;
    uint32_t win;
    bool waiting_for_input;
	bool waiting_for_win;
    string my_last_datagram;
    
    /* DATA FROM STDIN */
    bool eof;
    vector<char> ready_input;
    char stdin_buf[10000];
    boost::asio::posix::stream_descriptor input_stream;
       
    /* STATISTICS */
    bool waiting_for_ack;
    int waits;
    bool udp_active;
};
#endif
