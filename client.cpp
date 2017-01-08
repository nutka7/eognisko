#include <boost/bind.hpp>
#include <iostream>
#include <exception>
#include "client.h"
#include <assert.h>


namespace asio = boost::asio;

using std::istringstream;
using std::stringstream;
using std::cerr;
using std::cout;
using std::endl;
using std::getline;
using std::move;
using std::make_shared;
using std::make_pair;
using std::min;
using std::max;
using boost::posix_time::seconds;
using boost::posix_time::milliseconds;


Client::Client(ClientParams _params, asio::io_service & _io_service)
    : params(_params),
      io_service(_io_service),
      tcp_socket(io_service),
      udp_socket(io_service),
      keepalive_timer(io_service, seconds(0)),
      check_udp_timer(io_service, seconds(0)),
      nr_max_seen(0),
      nr_expected(0),
      next_ack(0),
      win(0),
      waiting_for_input(false),
      waiting_for_win(false),
      eof(false),
      input_stream(io_service, ::dup(STDIN_FILENO)),
      waiting_for_ack(false),
      waits(0),
      udp_active(false)
{
    establish_tcp_connection();
    setup_udp();
    receive_tcp(true); // embeds getting id and sending it
    schedule_keepalive();
    receive_udp();
    schedule_check_udp_active();
}

void Client::terminate() {
    io_service.stop();
}

void Client::establish_tcp_connection() {
    cerr << "Connecting to " << params.server_name << ":" << params.port  << "..." << endl;
    
    try {
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(params.server_name, std::to_string(params.port));
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        tcp::endpoint remote_endpoint = *endpoint_iterator;
        tcp_socket.connect(remote_endpoint);
        //asio::connect(tcp_socket, endpoint_iterator); -- only in newest boost
        cerr << "Connection established" << endl;
    } catch (std::exception & e) {
        cerr << "Couldn't connect to " << params.server_name << ":" << params.port << endl;
        terminate();
    }
}


void Client::setup_udp() {
    cerr << "Setting up udp socket..." << endl;

    try {
        udp::resolver resolver(io_service);
        udp::resolver::query query(params.server_name, std::to_string(params.port));
        udp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        udp::endpoint remote_endpoint = *endpoint_iterator;
        udp_socket.connect(remote_endpoint);
        //asio::connect(udp_socket, endpoint_iterator); -- only in newest boost
    } catch (std::exception &e) {
        cerr << "Couldn't set up UDP socket" << endl;
        terminate();
    }
}

void Client::schedule_keepalive() {
    keepalive_timer.expires_at(keepalive_timer.expires_at() + milliseconds(100)); 
    keepalive_timer.async_wait(
        boost::bind(
            &Client::keepalive,
            this,
            asio::placeholders::error)
   );
}

    
void Client::keepalive(const boost::system::error_code & ec) {
    schedule_keepalive();
    if (ec) {
        cerr << "timer error in keepalive\n"
             << ec.message() << endl;
        return;
    }

    auto header_p = make_shared<string>("KEEPALIVE\n");
    udp_socket.async_send(
        asio::buffer(*header_p),
        boost::bind(
            &Client::handle_keepalive,
            this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred,
            header_p)
    );
}

void Client::handle_keepalive(const boost::system::error_code& ec, size_t n,
                              shared_ptr<string> header_p)
{
   if (ec) {
       cerr << "error after keepalive\n"
            << ec.message() << endl;
   }
}

void Client::receive_tcp(bool initial) {
    tcp_socket.async_read_some(
        asio::buffer(tcp_rcv_buf),
        boost::bind(
            &Client::handle_receive_tcp,
             this,
             asio::placeholders::error,
             asio::placeholders::bytes_transferred,
             initial)
    );
}



void Client::handle_receive_tcp(const boost::system::error_code &ec, size_t n, bool initial) { 
    if (ec) {
        cerr << "problem with tcp connection\n"
             << ec.message() << endl;
        terminate();
        return;
    }
    string data(tcp_rcv_buf, n); 
    if (initial) {
        set_id_from_msg(data);
        send_id();
    } else {
        cerr << data;
    }
    receive_tcp(); 
} 

void Client::set_id_from_msg(string data) {
    try {
        istringstream istream(data);
        string command;
        istream >> command;
        if (command != "CLIENT") {
            cerr << "first tcp header received is not <<CLIENT>>\n";
            terminate();
            return;
        }
        istream >> id;
    } catch (std::exception& e) {
            cerr << "first tcp header received is not <<CLIENT>>\n";
            terminate();
            return;
    }
    cerr << "Received id: " << id << endl;
}

void Client::send_datagram(string data, bool set_waiting) {
    auto datagram_p = make_shared<string>(data);    

    udp_socket.async_send(
        asio::buffer(*datagram_p),
        boost::bind(
            &Client::handle_send_datagram,
            this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred,
            datagram_p,
            set_waiting)
    );
}

void Client::handle_send_datagram(const boost::system::error_code & ec, size_t n,
                                  shared_ptr<string> datagram_p, bool set_waiting) {
    if (ec) {
        cerr << "problem with sending datagram\n"
             << ec.message() << endl;
        terminate();
        return;
    }
    if (set_waiting) {
        waiting_for_ack = true;
        waits = 0;
    }
} 

void Client::send_id() {
    stringstream stream;
    stream << "CLIENT " << id << "\n";
    send_datagram(stream.str());
}

void Client::receive_udp() {
    udp_socket.async_receive(
        asio::buffer(udp_rcv_buf),
        boost::bind(
            &Client::handle_receive_udp,
            this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred)
    );
}

void Client::handle_receive_udp(const boost::system::error_code& ec, size_t n) {
    if (ec) {
        cerr << "error after receive_udp\n";
        receive_udp();
        return;
    }
    udp_active = true;

    char* newline_pos = std::find(udp_rcv_buf, udp_rcv_buf + n, '\n');
    if (newline_pos == udp_rcv_buf + n) {
        cerr << "no newline in received udp datagram\n";
        receive_udp();
        return;
    }
    
    char* header_start = udp_rcv_buf;
    char* data_start = newline_pos + 1;
    char* data_end = udp_rcv_buf + n;
    
    string header(header_start, data_start);
    string data(data_start, data_end);
    assert (data.size() == data_end - data_start);
    
    istringstream istream(header);
    string type;
    
    try {
        istream >> type;

        //std::cout << "NOWE UDP -- type: " << type << endl;
        if (type == "ACK") {
            uint32_t ack, win;
            istream >> ack >> win;
            handle_ack(ack, win);
        } else if (type == "DATA") {
            uint32_t nr, ack, win;
            istream >> nr >> ack >> win;
            handle_data_received(nr, ack, win, data);
        } else {
            cerr << "bad udp header from server\n";
        }
    } catch(std::exception& e) {
        cerr << "bad udp header from server\n";
        return;
    }
    receive_udp();
}


void Client::handle_ack(uint32_t ack, uint32_t _win, bool from_DATA) {
    if (waiting_for_win && _win > 0) {
		waiting_for_win = false;
		win = _win;
		upload_data();
		/* moglbym tutaj returnowac */
    }

    win = _win;
    if (ack >= next_ack) {
        next_ack = ack + 1;
        
        waiting_for_ack = false;
        waits = 0;
        
        upload_data();

    } else if (from_DATA && waiting_for_ack) {
        ++waits;
        if (waits >= 2) {
            waiting_for_ack = false;
            waits = 0;
            retransmit();
        }
    }
}

void Client::handle_data_received(uint32_t nr_recv, uint32_t ack, uint32_t _win, string data) {
    handle_ack(ack, _win, true);
    if (nr_recv == nr_expected || nr_expected + params.retransmit_limit < nr_recv) {
        nr_expected = nr_recv + 1;
        cout.write(data.data(), data.size());
	fflush(stdout);
    } else if (nr_recv > nr_max_seen) {
        ask_for_retransmit(nr_expected);
    }
    nr_max_seen = max(nr_max_seen, nr_recv);
}

void Client::upload_data() {
    if (win == 0) {
        waiting_for_win = true;
        return;
    }
    if (ready_input.size() == 0) {
        waiting_for_input = true;
    }

    else {
        //cerr << "Uploading..." << endl;
        uint32_t n = min(win, (uint32_t) 1400); /* UDP datagram shouldn't be too big */
        n = min((uint32_t) ready_input.size(), win);
        
        stringstream stream;
        stream << "UPLOAD " << next_ack - 1 << "\n";
        
        stream << string(ready_input.data(), n);
        ready_input.erase(ready_input.begin(), ready_input.begin() + n);
        
        my_last_datagram = stream.str();
        send_datagram(my_last_datagram, true);
    }
    if (ready_input.size() < 50000) {
    	read_stdin();
    }
}
    
void Client::ask_for_retransmit(uint32_t nr_expected) {
    stringstream stream;
    stream << "RETRANSMIT " << nr_expected << "\n";
    send_datagram(stream.str());
}
   
void Client::read_stdin() {
    if (eof) {
        return;
    }
    //cerr << "reading from stdin...\n";
    async_read(
            input_stream,
            asio::buffer(stdin_buf),
            boost::bind(
                &Client::handle_read_stdin,
                this,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred)
    );
}

void Client::handle_read_stdin(const boost::system::error_code &ec, size_t n) {
    if (ec && ec != asio::error::eof) {
        cerr << "stdin error\n" << ec.message() << endl;
        terminate();
        return;
    }
    if (ec == asio::error::eof) {
        eof = true;
        return;
    }
    //cerr << "Read from stdin succeded!\n";
    
    for (uint32_t i = 0; i < n; ++i) {
        ready_input.push_back(stdin_buf[i]);
    }

    if (waiting_for_input) {
        waiting_for_input = false;
        upload_data();
    }
}

void Client::schedule_check_udp_active() {
    check_udp_timer.expires_at(check_udp_timer.expires_at() + seconds(1)); 
    check_udp_timer.async_wait(
        boost::bind(
            &Client::check_udp_active,
            this,
            asio::placeholders::error)
   );
}
void Client::check_udp_active(const boost::system::error_code& ec) {
    schedule_check_udp_active();
    if (ec) {
        cerr << "timer error in check_udp_active\n"
             << ec.message() << endl;
        return;
    }
    if (udp_active) {
        udp_active = false;
    } else {
        cerr << "no udp datagram from server for one second\n";
        terminate();
    }
}

void Client::retransmit() {
    send_datagram(my_last_datagram);
}
