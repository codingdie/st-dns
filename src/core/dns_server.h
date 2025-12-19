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
#include "taskquque/task_queue.h"
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;
using namespace st::dns;
#define task_queue_param pair<string, remote_dns_server *>

static const uint64_t MAX_PRIORITY = 100;
class dns_server {
public:
    explicit dns_server(st::dns::config &config);

    void start();

    void wait_start();

    void shutdown();

private:
    atomic_uint64_t rid;
    st::dns::config config;
    udp::socket *ss = nullptr;
    io_context ic;
    boost::asio::io_context::work *iw;
    io_context schedule_ic;
    boost::asio::io_context::work *schedule_iw;
    boost::asio::deadline_timer *schedule_timer;
    std::atomic<uint8_t> state;
    atomic_int64_t counter;
    st::task::queue<pair<string, remote_dns_server *>> sync_remote_record_task_queue;

    void receive();

    void process_session(session *session);

    void query_dns_record(session *session, const std::function<void(st::dns::session *)> &complete_handler);

    void sync_dns_record_from_remote(const string &host);

    void sync_dns_record_from_remote(const string &host, const std::function<void(dns_record record)> &complete, remote_dns_server *server) const;

    void forward_dns_request(session *session, const std::function<void(st::dns::session *)> &complete_handler);

    void end_session(session *session);

    uint32_t cal_expire(dns_record &record) const;

    void start_console();

    dns_record query_record_from_cache(const string &host) const;

    void sync_loss_dns_record_from_remote(string &host, dns_record &record);

    void schedule();

    void start_console();
};


#endif//ST_DNS_DNS_SERVER_H
