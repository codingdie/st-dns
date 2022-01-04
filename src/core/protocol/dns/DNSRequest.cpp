//
// Created by codingdie on 2020/5/21.
//

#include "DNSRequest.h"
#include <chrono>
#include <vector>

void UdpDnsRequest::init(uint8_t *data, const vector<std::string> &hosts) {
    this->dnsHeader = DNSHeader::generateQuery(data, hosts.size());
    this->dnsQueryZone = DNSQueryZone::generate(data + this->dnsHeader->len, hosts);
    this->hosts = hosts;
    this->len = this->dnsHeader->len + this->dnsQueryZone->len;
}
UdpDnsRequest::UdpDnsRequest(const vector<std::string> &hosts) : BasicData(1024) {
    this->init(this->data, hosts);
}
UdpDnsRequest::UdpDnsRequest(uint8_t *data, const vector<std::string> &hosts) : BasicData(data) {
    this->init(this->data, hosts);
}
UdpDnsRequest::~UdpDnsRequest() {
    if (dnsHeader != nullptr) {
        delete dnsHeader;
    }
    if (dnsQueryZone != nullptr) {
        delete dnsQueryZone;
    }
}


bool UdpDnsRequest::parse(int n) {
    if (n > DNSHeader::DEFAULT_LEN) {
        DNSHeader *header = DNSHeader::parse(data, DNSHeader::DEFAULT_LEN);
        if (header != nullptr) {
            dnsHeader = header;
            DNSQueryZone *queryZone = new DNSQueryZone(data + DNSHeader::DEFAULT_LEN, n - DNSHeader::DEFAULT_LEN, header->questionCount);
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
    this->len = n;
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

uint16_t UdpDnsRequest::getQueryTypeValue() const {
    return this->dnsQueryZone->querys[0]->queryTypeValue;
}

UdpDnsRequest::UdpDnsRequest() {
}

TcpDnsRequest::TcpDnsRequest(const vector<std::string> &hosts) : UdpDnsRequest(1024 + SIZE_HEADER) {
    this->init(this->data + 2, hosts);

    uint8_t lenArr[2];
    uint16_t dataLen = this->len;
    st::utils::toBytes(lenArr, dataLen);
    st::utils::copy(lenArr, data, 0U, 0, SIZE_HEADER);
    this->len = this->len + SIZE_HEADER;
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
