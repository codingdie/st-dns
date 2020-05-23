//
// Created by codingdie on 2020/5/21.
//

#include "DnsRequest.h"
#include <chrono>
#include <vector>

UdpDnsRequest::UdpDnsRequest(vector<std::string> &hosts) {
    this->data = data;
    this->dnsHeader = DNSHeader::generateQuery(hosts.size());
    this->dnsQueryZone = DNSQueryZone::generateQuery(hosts);
    initDataZone();
}

void UdpDnsRequest::initDataZone() {
    uint16_t len = dnsHeader->len + dnsQueryZone->len;
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
    BasicData::len = len;
    BasicData::data = data;
}

UdpDnsRequest::~UdpDnsRequest() {
    delete dnsHeader;
    delete dnsQueryZone;
}

TcpDnsRequest::TcpDnsRequest(vector<std::string> &hosts) {
    this->data = data;
    this->dnsHeader = DNSHeader::generateQuery(hosts.size());
    this->dnsQueryZone = DNSQueryZone::generateQuery(hosts);
    initDataZone();
}

void TcpDnsRequest::initDataZone() {
    uint16_t len = dnsHeader->len + dnsQueryZone->len + 2;
    byte lenArr[2];
    uint16_t dataLen = len - 2;
    toBytes(lenArr, dataLen);
    byte *data = new byte[len];
    uint32_t pos = 0;
    copy(lenArr, data, 0U, pos, 2U);
    pos += 2;
    copy(dnsHeader->data, data, 0U, pos, dnsHeader->len);
    pos += dnsHeader->len;
    copy(dnsQueryZone->data, data, 0U, pos, dnsQueryZone->len);
    BasicData::len = len;
    BasicData::data = data;
}
