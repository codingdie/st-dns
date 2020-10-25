//
// Created by codingdie on 2020/5/21.
//

#include "DNSRequest.h"
#include <chrono>
#include <vector>

UdpDnsRequest::UdpDnsRequest(const vector<std::string> &hosts) {
    this->data = data;
    this->dnsHeader = DNSHeader::generateQuery(hosts.size());
    this->dnsQueryZone = DNSQueryZone::generate(hosts);
    this->hosts = hosts;
    initDataZone();
}

void UdpDnsRequest::initDataZone() {
    uint16_t len = dnsHeader->len + dnsQueryZone->len;
    uint8_t *data = new uint8_t[len];
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
                    markInValid();
                } else {
                    dnsQueryZone = queryZone;
                    for (auto it : queryZone->querys) {
                        if (!it->domain->domain.empty()) {
                            this->hosts.emplace_back(it->domain->domain);
                        }
                    }
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

UdpDnsRequest::UdpDnsRequest(uint8_t *data, uint64_t len, bool dataOwner) : BasicData(data, len) {
    this->setDataOwner(dataOwner);
}

UdpDnsRequest::UdpDnsRequest(uint64_t len) : BasicData(len) {
}

string UdpDnsRequest::getHost() const {
    if (hosts.empty()) {
        return "";
    }
    return hosts.front();
}
DNSQuery::Type UdpDnsRequest::getQueryType() const {
    return this->dnsQueryZone->querys[0]->queryType;
}

UdpDnsRequest::UdpDnsRequest() {
}

TcpDnsRequest::TcpDnsRequest(const vector<std::string> &hosts) {
    this->dnsHeader = DNSHeader::generateQuery(hosts.size());
    this->dnsQueryZone = DNSQueryZone::generate(hosts);
    initDataZone();
}


TcpDnsRequest::TcpDnsRequest(const vector<std::string> &hosts, uint32_t ip) {
    this->dnsHeader = DNSHeader::generate(hosts.size(), 1, 0, 0, false);
    this->dnsQueryZone = DNSQueryZone::generate(hosts);
    this->ENDSZone = EDNSAdditionalZone::generate(ip);
    initDataZone();
}

void TcpDnsRequest::initDataZone() {
    uint16_t len = dnsHeader->len + dnsQueryZone->len + 2;
    if (ENDSZone != nullptr) {
        len += ENDSZone->len;
    }
    uint8_t lenArr[2];
    uint16_t dataLen = len - 2;
    st::utils::toBytes(lenArr, dataLen);
    uint8_t *data = new uint8_t[len];
    uint32_t pos = 0;
    st::utils::copy(lenArr, data, 0U, pos, 2U);
    pos += 2;
    st::utils::copy(dnsHeader->data, data, 0U, pos, dnsHeader->len);
    pos += dnsHeader->len;
    st::utils::copy(dnsQueryZone->data, data, 0U, pos, dnsQueryZone->len);
    pos += dnsQueryZone->len;
    if (ENDSZone != nullptr) {
        st::utils::copy(ENDSZone->data, data, 0U, pos, ENDSZone->len);
    }
    BasicData::len = len;
    BasicData::data = data;
    BasicData::setDataOwner(true);
}

TcpDnsRequest::~TcpDnsRequest() {
    if (ENDSZone != nullptr) {
        delete ENDSZone;
    }
}

EDNSAdditionalZone *EDNSAdditionalZone::generate(uint32_t ip) {
    int size = 23;
    EDNSAdditionalZone *zone = new EDNSAdditionalZone(size);
    uint8_t *data = zone->data;
    //name empty
    data[0] = 0;
    //type
    data[1] = 0;
    data[2] = 41;
    //udp size
    data[3] = 0x10;
    data[4] = 0;

    //header
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    data[8] = 0;
    //len
    data[9] = 0;
    data[10] = 12;
    //data
    data[11] = 0;
    data[12] = 0x08;

    //len
    data[13] = 0;
    data[14] = 8;
    //family
    data[15] = 0;
    data[16] = 1;

    //address len
    data[17] = 0;
    data[18] = 0;
    //ipv4
    data[19] = (ip >> (8 * 3)) & 0xFF;
    data[20] = (ip >> (8 * 2)) & 0xFF;
    data[21] = (ip >> (8 * 1)) & 0xFF;
    data[22] = ip & 0xFF;
    return zone;
}

EDNSAdditionalZone::EDNSAdditionalZone(uint32_t len) : BasicData(len) {
}
