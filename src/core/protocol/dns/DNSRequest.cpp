//
// Created by codingdie on 2020/5/21.
//

#include "DNSRequest.h"
#include <chrono>
#include <vector>


DNSRequest::DNSRequest(const std::string &host) {
    this->dnsHeader = DNSHeader::generateQuery();
    this->dnsQueryZone = DNSQueryZone::generateQuery(host);
    unsigned int len = dnsHeader->len + dnsQueryZone->len;
    byte *data = new byte[len];
    int j = 0;
    for (int i = 0; i < dnsHeader->len; i++) {
        data[j] = dnsHeader->data[i];
        j++;
    }
    for (int i = 0; i < dnsQueryZone->len; i++) {
        data[j] = dnsQueryZone->data[i];
        j++;
    }
    this->data = data;
    this->len = len;
}

DNSRequest::~DNSRequest() {
    delete dnsHeader;
    delete dnsQueryZone;
}
