//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_DNS_SERVER_H
#define ST_DNS_DNS_SERVER_H

#include "config.h"
#include "dns_record_manager.h"
#include "session.h"
#include "st.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;
using namespace st::dns;

class dns_server {
public:
    dns_server(st::dns::config &config);

    void start();

    void wait_start();

    void shutdown();

private:
    atomic_int64_t rid;
    st::dns::config config;
    udp::socket *ss = nullptr;
    io_context ic;
    boost::asio::io_context::work *iw;
    unordered_map<string, unordered_set<session *>> wating_sessions;
    std::atomic<uint8_t> state;
    atomic_int64_t counter;
    st::console::udp_console console;

    void receive();

    void process_session(session *session);

    void query_dns_record(session *session, const std::function<void(st::dns::session *)> &complete_handler);

    void sync_dns_record_from_remote(const string &host, const std::function<void(dns_record record)> &complete, vector<remote_dns_server *> servers, int pos, bool completed);

    static void filter_ip_by_area(const string &host, remote_dns_server *server, vector<uint32_t> &ips);

    void query_dns_record_from_remote(session *session, const std::function<void(st::dns::session *)> &complete_handler);

    void forward_dns_request(session *session, const std::function<void(st::dns::session *)> &complete_handler);

    void forward_dns_request(session *session, const std::function<void(protocol::udp_response *)> &complete, vector<remote_dns_server *> servers, uint32_t pos);

    void end_session(session *session);

    void update_dns_record(const string &domain);

    bool begin_query_remote(const string &domain, session *session);

    unordered_set<session *> end_query_remote(const string &host);

    uint32_t cal_expire(dns_record &record) const;
    void config_console();
};


#endif//ST_DNS_DNS_SERVER_H
