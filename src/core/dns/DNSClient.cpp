//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"


#include <iostream>
#include "DNSClient.h"
#include "DNS.h"
#include <mutex>

static std::atomic_int count1;

std::string DNSClient::udpDns(string &domain, string &dnsServer) {
    query(dnsServer, domain);
    return "";

}


std::string DNSClient::query(const string &dnsServer, const string &domain) {
    boost::asio::io_context ioContext;
    udp::socket socket(ioContext, udp::endpoint(udp::v4(), 0));
    DNSRequest dnsRequest(domain);
    byte reply[1024];
    udp::endpoint serverEndpoint;
    deadline_timer timeout(ioContext);
    unsigned short rid = 0;
    unsigned short qid = dnsRequest.dnsHeader->id;

    socket.async_send_to(buffer(dnsRequest.data, dnsRequest.len), udp::endpoint(make_address_v4(dnsServer), 53),
                         [&](boost::system::error_code, std::size_t) {
                             socket.async_receive_from(buffer(reply, sizeof(unsigned char) * 1024), serverEndpoint,
                                                       [&](boost::system::error_code, std::size_t len) {
                                                           DNSResponse response(reply, len);
                                                           rid = response.header->id;
                                                           response.print();
                                                           if (rid == qid) {
                                                               timeout.cancel();
                                                           }

                                                       });
                         });
    timeout.expires_from_now(boost::posix_time::milliseconds(300));
    timeout.async_wait([&](const boost::system::error_code &error) {
        string logStr = domain + "\t" + to_string(qid) + "\t";

        if (error == boost::asio::error::operation_aborted) {
            logStr = logStr + "success\t" + to_string(rid);
        } else {
            logStr = logStr + "failed ";
            socket.cancel();
        }
        LogUtils::info(logStr);

    });
    ioContext.run();


    return "";

}
