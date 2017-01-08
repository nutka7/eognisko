#include <boost/bind.hpp>
#include <iostream>
#include <exception>
#include "server.h"


namespace asio = boost::asio;

using std::istringstream;
using std::stringstream;
using std::cerr;
using std::endl;
using std::getline;
using std::move;
using std::make_shared;
using std::make_pair;
using boost::posix_time::seconds;
using boost::posix_time::milliseconds;
/*
const boost::regex server::CLIENT("CLIENT (0|[1-9][0-9]{0,8})\n");
const boost::regex server::UPLOAD("UPLOAD (0|[1-9][0-9]*)\n(.*)");
const boost::regex server::KEEPALIVE("KEEPALIVE\n");
const boost::regex server::RETRANSMIT("RETRANSMIT (0|[1-9][0-9]*)\n");
*/

Server::Server(ServerParams _params,
               asio::io_service & _io_service) 
    : params(_params), 
      io_service(_io_service),
      acceptor(io_service, tcp::endpoint(tcp::v6(), params.port)),
      udp_socket(io_service, udp::endpoint(udp::v6(), params.port)),
      report_timer(io_service, seconds(0)),
      remove_bad_sessions_timer(io_service, seconds(0)),
      mix_and_send_timer(io_service, seconds(0)),
      next_free_id(0),
      remix_nr(0)
{
    schedule_report();
    schedule_remove_bad_sessions();
    schedule_mix_and_send();
    accept_tcp();
    receive_udp();
}

/* REPORTS */

void Server::schedule_report() {
    report_timer.expires_at(report_timer.expires_at() + seconds(1));
    report_timer.async_wait(
        boost::bind(
            &Server::report,
            this,
            asio::placeholders::error));
}

void Server::report(const boost::system::error_code& ec) {
    schedule_report();
    if (ec) {
        cerr << "timer error in report\n"
             << ec.message() << endl;
        return;
    }

    /* Report message must be passed using shared_ptr to make sure it persits
     * till all operations referring to it finish. */
    auto report_p = make_shared<string>(construct_report());
    
    multi_send_report(report_p);
    multi_reset_fifo_stats();
}

void Server::multi_send_report(shared_ptr<string> report_p) {
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        auto session_p = it->second;
        send_report(session_p, report_p);
    }
}

void Server::multi_reset_fifo_stats() {
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        auto session_p = it->second;
        session_p->reset_fifo_stats();
    }
}

string Server::construct_report() {
    stringstream stream;
    stream << "\n";
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        Session & session = *(it->second);
        if (session.uses_udp) {
            stream << session.get_info();
        }
    }
    return stream.str();
}

void Server::send_report(shared_ptr<Session> session_p, shared_ptr<string> report_p) {
    Session & session = *session_p;
    asio::async_write(
        *session.tcp_socket_p, 
        asio::buffer(*report_p),
        boost::bind(
            &Server::handle_send_report, 
            this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred,
            session_p,
            report_p)
    );
}

void Server::handle_send_report(const boost::system::error_code& ec, size_t n,
                                shared_ptr<Session> session_p, shared_ptr<string> report_p)
{
   if (ec) {
       cerr << "TCP problem -- id: " << session_p->id << "\n"
            << ec.message() << endl;

       remove_session(session_p);
   }
}

/* BAD SESSIONS REMOVAL */

void Server::remove_session(shared_ptr<Session> session_p) {
    auto id = session_p->id;
    cerr << "removing session -- id: " << id;

    auto it = sessions.find(id);
    if (it != sessions.end()) {
        if (session_p->uses_udp) {
            endpoint_to_session.erase(session_p->udp_remote_endpoint);
        }
        sessions.erase(it);
    } else {
        cerr << "session removed earlier! -- id: " << session_p->id;
    }
    cerr << "\n";
}

void Server::schedule_remove_bad_sessions() {
    remove_bad_sessions_timer.expires_at(remove_bad_sessions_timer.expires_at() + seconds(1));
    remove_bad_sessions_timer.async_wait(
        boost::bind(
            &Server::remove_bad_sessions,
            this,
            asio::placeholders::error)
        );
}

void Server::remove_bad_sessions(const boost::system::error_code& ec) {
    schedule_remove_bad_sessions();
    if (ec) {
        cerr << "timer error in remove_bad_sessions\n"
             << ec.message() << endl;
        return;
    }
    
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        auto session_p = it->second;
        Session & session = *session_p;
        if (session.uses_udp) {
            if (session.udp_alive) {
                session.reset_alive_stats();
            } else {
                cerr << "udp not alive -- id: " << session_p->id << "\n";
                remove_session(session_p);
            }
        }
    }
}

/* MIXING AND SENDING */

void Server::schedule_mix_and_send() { 
    mix_and_send_timer.expires_at(mix_and_send_timer.expires_at() + milliseconds(params.tx_interval));
    mix_and_send_timer.async_wait(
        boost::bind(
            &Server::mix_and_send,
            this,
            asio::placeholders::error)
        );
}

void Server::mix_and_send(const boost::system::error_code& ec) {
    schedule_mix_and_send();
    if (ec) {
        cerr << "timer error mix_and_send\n"
             << ec.message() << endl;
        return;
    }

    ++remix_nr;
    remixes.insert(make_pair(remix_nr, mix()));
    if (remixes.size() > params.buf_len) {
        remixes.erase(remix_nr - params.buf_len);
    }
    multi_send_remix_datagram(remix_nr);
}

string Server::mix() {
    vector<mixer_input> inputs = construct_mixer_inputs();
    size_t output_size = OUTPUT_BUF_SIZE;
    mixer(inputs.data(), inputs.size(), (void*) output_buf, &output_size, params.tx_interval);
    multi_consume(inputs);
    return string((const char*) output_buf, output_size);
}


vector<mixer_input> Server::construct_mixer_inputs() {
    vector<mixer_input> inputs;
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        Session & session = *(it->second);
        if (session.fifo_state == ACTIVE) {
            auto input_data = session.get_data();
            struct mixer_input input {input_data.first, input_data.second, 0};
            inputs.push_back(input);
        }
    }
    return inputs;
}

void Server::multi_consume(vector<mixer_input> & inputs) {
    int counter = 0;
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        Session & session = *(it->second);
        if (session.fifo_state == ACTIVE) {
            session.consume(inputs[counter].consumed);
            ++counter;
        }
    }
}

void Server::multi_send_remix_datagram(uint32_t nr) {
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        auto session_p = it->second;
        if (session_p->uses_udp) {
            send_remix_datagram(session_p, nr);
        }
    }
}

void Server::send_remix_datagram(shared_ptr<Session> session_p, uint32_t nr) {
    udp::endpoint remote_endpoint = session_p->udp_remote_endpoint;
    auto datagram_p = make_shared<string>(construct_remix_datagram(session_p, nr));

    //std::cout << "sending datagram to: " << remote_endpoint << endl;
    //          <<  *datagram_p << "--end_of_data" << endl;
    
    udp_socket.async_send_to(
        asio::buffer(*datagram_p),
        remote_endpoint,
        boost::bind(
            &Server::handle_send_remix_datagram,
            this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred,
            session_p,
            datagram_p)
        );

}

string Server::construct_remix_datagram(shared_ptr<Session> session_p, uint32_t nr) {
    stringstream stream;
    stream << session_p->get_datagram_header(nr)
           << remixes[nr];

    return stream.str();
}
    
void Server::handle_send_remix_datagram(const boost::system::error_code& ec, size_t n,
                                        shared_ptr<Session> session_p, shared_ptr<string> datagram_p)
{
   if (ec) {
       cerr << "error after send_remix_datagram -- id: " << session_p->id
            << "\n" << ec.message() << endl;
   }
}

/* ACCEPTING TCP */

void Server::accept_tcp() {
    auto tcp_socket_p = make_shared<tcp::socket>(io_service);
    acceptor.async_accept(
        *tcp_socket_p,
        boost::bind(
            &Server::handle_accept_tcp,
            this,
            asio::placeholders::error,
            tcp_socket_p)
    );
}
    
void Server::handle_accept_tcp(const boost::system::error_code& ec, shared_ptr<tcp::socket> tcp_socket_p) {
    if (ec) {
        cerr << "error after accept_tcp\n";
        accept_tcp();
        return;
    }
    cerr << "accepted new session -- id: " << next_free_id << "\n";
    
    auto new_session_p = make_shared<Session>(next_free_id, params, tcp_socket_p);
    sessions.insert(make_pair(next_free_id, new_session_p));
    
    ++next_free_id;
    send_client_message(new_session_p);
    accept_tcp();
}

void Server::send_client_message(shared_ptr<Session> session_p) {
    auto msg_p = make_shared<string>(session_p->get_client_header());
    send_report(session_p, msg_p);
}
    


/* ACCEPTING UDP */
void Server::receive_udp() {
    udp_socket.async_receive_from(
        boost::asio::buffer(recv_buf),
        udp_remote_endpoint,
        boost::bind(
            &Server::handle_receive_udp,
            this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred)
    );
}

void Server::handle_receive_udp(const boost::system::error_code& ec, size_t n) {
    if (ec) {
        cerr << "error after receive_udp\n";
        receive_udp();
        return;
    }

    char* newline_pos = std::find(recv_buf, recv_buf + n, '\n');
    if (newline_pos == recv_buf + n) {
        cerr << "no newline in received udp datagram\n";
        receive_udp();
        return;
    }
    
    char* header_start = recv_buf;
    char* data_start = newline_pos + 1;
    char* data_end = recv_buf + n;
    
    string header(header_start, data_start);
    string data(data_start, data_end);
    
    istringstream istream(header);
    string type;
    
    try {
        istream >> type;

        //std::cout << "NOWE UDP -- type: " << type << endl;
        if (type == "CLIENT") {
            uint32_t id;
            istream >> id;
            client(udp_remote_endpoint, id);
        } else if (type == "UPLOAD") {
            uint32_t nr;
            istream >> nr;
            upload(udp_remote_endpoint, data, nr);
        } else if (type == "RETRANSMIT") {
            uint32_t nr;
            istream >> nr;
            retransmit(udp_remote_endpoint, nr);
        } else if (type == "KEEPALIVE") {
            keepalive(udp_remote_endpoint);
        } else {
            cerr << "bad udp header from: " << udp_remote_endpoint << endl;
        }
    } catch(std::exception& e) {
        cerr << "bad udp header from: " << udp_remote_endpoint << endl;
        return;
    }
    receive_udp();
}

void Server::client(udp::endpoint endpoint, uint32_t id) {
    auto it = sessions.find(id);
    if (it != sessions.end()) {
        auto session_p = it->second;
        session_p->init_udp(endpoint);
        endpoint_to_session.insert(make_pair(endpoint, session_p));
        send_ack(session_p);
    } else {
        cerr << "not existing client tried to init udp -- id: " << id << endl;
    }
}

void Server::retransmit(udp::endpoint endpoint, uint32_t nr) {
    auto it = endpoint_to_session.find(endpoint);
    if (it == endpoint_to_session.end()) {
        cerr << "unknown udp endpoint asking for retransmit: "
             << endpoint << "\n";
        return;
    }
    auto session_p = it->second;
    session_p->keepalive();
    
    uint32_t min_avaiable = remix_nr + 1 - remixes.size();
    uint32_t start = std::max(nr, min_avaiable);
    
    for (uint32_t i = start; i <= remix_nr; ++i) {
        send_remix_datagram(session_p, i);
    }
}

void Server::upload(udp::endpoint endpoint, string data, uint32_t nr) {
    /* NA TEST */
    //std::cout << "UPLOAD: " << endpoint << endl;
    auto it = endpoint_to_session.find(endpoint);
    if (it == endpoint_to_session.end()) {
        cerr << "unknown udp endpoint wants to upload: "
             << endpoint << "\n";
        return;
    }
    auto session_p = it->second;
    session_p->keepalive();

    if (nr != session_p->ack) {
        cerr << "Upload with bad nr: " << nr
             << " expected ack: " << session_p->ack
             << " -- id: " << session_p->id << "\n";
        return;
    }
    if (data.size() > session_p->get_win()) {
        cerr << "Upload too big, size: " << data.size()
             << " win: " << session_p->get_win() << " -- id " << session_p->id << "\n";
        return;
    }
    //std::cout << "DATA: " << data << endl;
    session_p->upload(data);
    send_ack(session_p);
}

void Server::keepalive(udp::endpoint endpoint) {
    auto it = endpoint_to_session.find(endpoint);
    if (it == endpoint_to_session.end()) {
        cerr << "unknown udp endpoint asking for keepalive\n";
        return;
    }
    auto session_p = it->second;
    session_p->keepalive();
}

void Server::send_ack(shared_ptr<Session> session_p) {
     auto ack_msg_p = make_shared<string>(session_p->get_ack_header());
     
     udp_socket.async_send_to(
        asio::buffer(*ack_msg_p),
        session_p->udp_remote_endpoint,
     boost::bind(
         &Server::handle_send_ack,
         this,
         boost::asio::placeholders::error,
         boost::asio::placeholders::bytes_transferred,
         session_p,
         ack_msg_p)
     );
}

void Server::handle_send_ack(const boost::system::error_code& ec, size_t n, shared_ptr<Session> session_p, shared_ptr<string> ack_msg_p) {
    if (ec) {
        cerr << "error after send_ack -- id: " << session_p->id << endl;
        return;
    }
}

