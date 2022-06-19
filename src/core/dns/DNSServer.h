//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_DNSSERVER_H
#define ST_DNS_DNSSERVER_H

#include "config.h"
#include "dns_cache.h"
#include "session.h"
#include "utils/utils.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;
using namespace st::dns;

class DNSServer {
public:
    DNSServer(st::dns::config &config);

    void start();

    void waitStart();

    void shutdown();

private:
    atomic_int64_t rid;
    st::dns::config config;
    udp::socket *socketS = nullptr;
    io_context ioContext;
    boost::asio::io_context::work *ioWoker;
    unordered_map<string, unordered_set<session *>> watingSessions;
    std::atomic<uint8_t> state;
    atomic_int64_t counter;

    void receive();

    void processSession(session *session);

    void queryDNSRecord(session *session, std::function<void(st::dns::session *)> completeHandler);

    void syncDNSRecordFromServer(const string host, std::function<void(dns_record record)> complete, vector<remote_dns_server *> servers, int pos, bool completed);

    void filterIPByArea(const string host, remote_dns_server *server, vector<uint32_t> &ips);

    void queryDNSRecordFromServer(session *session, std::function<void(st::dns::session *)> completeHandler);

    void forwardUdpDNSRequest(session *session, std::function<void(st::dns::session *)> completeHandler);

    void forwardUdpDNSRequest(session *session, std::function<void(udp_response *)> complete, vector<remote_dns_server *> servers, uint32_t pos);

    void endDNSSession(session *session);

    void updateDNSRecord(dns_record record);

    void calRemoteDNSServers(const dns_record &record, vector<remote_dns_server *> &servers);

    bool beginQueryRemote(const string host, session *session);

    unordered_set<session *> endQueryRemote(const string host);

    uint32_t getExpire(dns_record &record);
};


#endif//ST_DNS_DNSSERVER_H
