#include <iostream>
#include "session.h"

using std::make_pair;
using std::min;
using std::max;
using std::stringstream;


Session::Session(uint32_t _id,
                 ServerParams& _params,
                 shared_ptr<tcp::socket> _tcp_socket_p)
    : id(_id),
      params(_params),
      tcp_socket_p(_tcp_socket_p),
      tcp_remote_endpoint(tcp_socket_p->remote_endpoint()),
      uses_udp(false),
      ack(0),
      fifo_state(FILLING),
      fifo_max(0),
      fifo_min(0),
      udp_alive(false) {}


string Session::get_info()
{
    stringstream stream;

    stream << tcp_remote_endpoint << " "
           << "FIFO: " << fifo.size() << "/" << params.fifo_size << " "
           << "(min. " << fifo_min << ", max. " << fifo_max << ")\n";

    return stream.str();
}

string Session::get_datagram_header(uint32_t nr) {
    stringstream stream;

    stream << "DATA " << nr << " " << ack << " " 
           << get_win() << "\n";

    return stream.str();
}

string Session::get_ack_header() {
    stringstream stream;

    stream << "ACK " << ack << " " << get_win() << "\n";

    return stream.str();
}

string Session::get_client_header() {
    stringstream stream;

    stream << "CLIENT " << id << "\n";

    return stream.str();
}

void Session::reset_fifo_stats()
{
    fifo_min = fifo.size();
    fifo_max = fifo.size();
}

void Session::reset_alive_stats()
{
    udp_alive = false;
}

pair<void*, size_t> Session::get_data()
{
    return make_pair((void*) fifo.data(), fifo.size());
}

void Session::consume(size_t bytes) 
{
    size_t bytes_to_erase = min(bytes, fifo.size());
    fifo.erase(fifo.begin(), fifo.begin() + bytes_to_erase);
    fifo_min = min(fifo_min, fifo.size());
    if (fifo.size() <= params.fifo_low_watermark) {
        fifo_state = FILLING;
    }
}

void Session::init_udp(udp::endpoint remote_endpoint) {
    udp_remote_endpoint = remote_endpoint;
    uses_udp = true;
    keepalive();
}

void Session::keepalive() {
    udp_alive = true;
}

void Session::upload(string data) {
    ++ack;
    for (size_t i = 0; i < data.size(); ++i) {
        fifo.push_back(data[i]);
    }
    fifo_max = max(fifo.size(), fifo_max);
    if (fifo.size() >= params.fifo_high_watermark) {
        fifo_state = ACTIVE;
    }
}

size_t Session::get_win() {
    return params.fifo_size - fifo.size();
}
