//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_DNSSERVER_H
#define ST_DNS_DNSSERVER_H

#include "Config.h"
#include "DNSCache.h"
#include "DNSSession.h"
#include "STUtils.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

class DNSServer {
public:
    DNSServer(st::dns::Config &config);

    void start();

private:
    atomic_int64_t rid;
    st::dns::Config config;
    udp::socket *socketS = nullptr;
    io_context ioContext;
    unordered_map<string, thread_pool *> workThreads;

    void receive();

    void processSession(DNSSession *session);

    void queryDNSRecord(DNSSession *session, std::function<void(DNSSession *)> completeHandler);

    void syncDNSRecordFromServer(const string host, std::function<void(DNSRecord &record)> complete, vector<RemoteDNSServer *> servers, int pos);

    void filterIPByArea(const string host, RemoteDNSServer *server, unordered_set<uint32_t> &ips);

    void queryDNSRecordFromServer(DNSSession *session, std::function<void(DNSSession *session)> completeHandler);

    void forwardUdpDNSRequest(DNSSession *session, std::function<void(DNSSession *)> completeHandler);
    void forwardUdpDNSRequest(DNSSession *session, std::function<void(UdpDNSResponse *)> complete, vector<RemoteDNSServer *> servers, int pos);

    void endDNSSession(const DNSSession *session) const;

    void updateDNSRecord(DNSRecord record);

    void calRemoteDNSServers(const DNSRecord &record, vector<RemoteDNSServer *> &servers);

    bool beginQuery(const string host);

    void endQuery(const string host);
};


#endif//ST_DNS_DNSSERVER_H
