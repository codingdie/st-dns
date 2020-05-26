//
// Created by codingdie on 2020/5/20.
//

#include <iostream>
#include "DNSClient.h"

#include <thread>
#include <vector>
#include <chrono>


using namespace std;
using namespace std::chrono;
using namespace boost::asio;

long now() {

    auto time_now = std::chrono::system_clock::now();
    auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
    return duration_in_ms.count();
}

template<typename L>
void testParallel(L getDns) {
    vector<thread *> threads;
    long begin = now();
    int total = 1;
    for (int i = 0; i < total; i++) {
        auto func = [getDns]() {
            string domain = "music.163.com";
            string server = "8.8.8.8";
            getDns(domain, server);
        };
        thread *thread = new class thread(func);
        threads.emplace_back(thread);
    }
    int i = 0;
    for (auto it = threads.begin(); it != threads.end(); it++) {
        thread *pThread = *it.base();
        pThread->join();
        delete pThread;
    }
    long end = now();
    long d = (end - begin);
    cout << d << "\t" << d * 1.0 / total << endl;
}


int main(int argc, char *argv[]) {
//    testParallel([](string &domain, string &server) {
//        auto tcpDnsResponse = DNSClient::tcpDns(domain, server);
//        if (tcpDnsResponse != nullptr) {
//            UdpDNSResponse *dnsResponse = tcpDnsResponse->udpDnsResponse;
//            Logger::INFO << "success" << dnsResponse->header->id
//                         << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id
//                         << ipsToStr(dnsResponse->ips) << END;
//            delete tcpDnsResponse;
//        }
//    });
    testParallel([](string &domain, string &server) {
        auto tcpDnsResponse = DNSClient::tcpDns(domain, server);
        if (tcpDnsResponse != nullptr) {
            UdpDNSResponse *dnsResponse = tcpDnsResponse->udpDnsResponse;
            Logger::INFO << "success" << dnsResponse->header->id
                         << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id
                         << ipsToStr(dnsResponse->ips) << END;
            delete tcpDnsResponse;
        }
    });
}
