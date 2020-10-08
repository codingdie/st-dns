//
// Created by codingdie on 2020/5/21.
//

#include "DNSRequest.h"
#include <chrono>
#include <vector>

UdpDnsRequest::UdpDnsRequest(const vector<std::string> &hosts) {
    this->data = data;
    this->dnsHeader = DNSHeader::generate(hosts.size());
    this->dnsQueryZone = DNSQueryZone::generate(hosts);
    this->hosts = hosts;
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
    if (dnsHeader != nullptr) {
        delete dnsHeader;
    }
    if (dnsQueryZone != nullptr) {
        delete dnsQueryZone;
    }
}


bool UdpDnsRequest::parse() {
    if (len > DNSHeader::DEFAULT_LEN) {
        DNSHeader *header = new DNSHeader(data, DNSHeader::DEFAULT_LEN);
        if (header->isValid()) {
            dnsHeader = header;
            DNSQueryZone *queryZone = new DNSQueryZone(data + DNSHeader::DEFAULT_LEN, len - DNSHeader::DEFAULT_LEN, header->questionCount);
            if (queryZone->isValid()) {
                if (queryZone->querys.size() == 0) {
                    this->printHex();
                    cout << "123" << endl;
                }
                dnsQueryZone = queryZone;
                for (auto it:queryZone->querys) {
                    this->hosts.emplace_back(it->domain->domain);
                }
            } else {
                markInValid();
            }
        } else {
            markInValid();
        }
    } else {
        markInValid();
    }
    return isValid();
}

UdpDnsRequest::UdpDnsRequest(byte *data, uint64_t len, bool dataOwner) : BasicData(data, len) {
    this->setDataOwner(dataOwner);
}

UdpDnsRequest::UdpDnsRequest(uint64_t len) : BasicData(len) {

}

string UdpDnsRequest::getFirstHost() {
    return hosts.front();
}

UdpDnsRequest::UdpDnsRequest() {
}

TcpDnsRequest::TcpDnsRequest(const vector<std::string> &hosts) {
    this->dnsHeader = DNSHeader::generate(hosts.size());
    this->dnsQueryZone = DNSQueryZone::generate(hosts);
    initDataZone();
}

void TcpDnsRequest::initDataZone() {
    uint16_t len = dnsHeader->len + dnsQueryZone->len + 2;
    byte lenArr[2];
    uint16_t dataLen = len - 2;
    st::utils::toBytes(lenArr, dataLen);
    byte *data = new byte[len];
    uint32_t pos = 0;
    st::utils::copy(lenArr, data, 0U, pos, 2U);
    pos += 2;
    st::utils::copy(dnsHeader->data, data, 0U, pos, dnsHeader->len);
    pos += dnsHeader->len;
    st::utils::copy(dnsQueryZone->data, data, 0U, pos, dnsQueryZone->len);
    BasicData::len = len;
    BasicData::data = data;
    BasicData::setDataOwner(true);
}
