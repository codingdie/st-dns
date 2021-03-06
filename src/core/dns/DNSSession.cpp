//
// Created by codingdie on 2020/5/28.
//

#include "DNSSession.h"

DNSSession::~DNSSession() {
    if (udpDNSResponse != nullptr) {
        delete udpDNSResponse;
    }
}

DNSSession::DNSSession(uint64_t id) : id(id), stepLogger("st-dns", to_string(id)), udpDnsRequest(1024) {
}

uint64_t DNSSession::getId() const {
    return id;
}

string DNSSession::getHost() const {
    return udpDnsRequest.getHost();
}
DNSQuery::Type DNSSession::getQueryType() const {
    return udpDnsRequest.getQueryType();
}

uint16_t DNSSession::getQueryTypeValue() const {
    return udpDnsRequest.getQueryTypeValue();
}


uint64_t DNSSession::getTime() const {
    return time;
}

void DNSSession::setTime(uint64_t time) {
    DNSSession::time = time;
}
