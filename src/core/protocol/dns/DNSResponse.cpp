//
// Created by codingdie on 2020/5/22.
//

#include "DNSResponse.h"


void UdpDNSResponse::print() const {
    BasicData::print();

    if (isValid()) {
        cout << "id:" << this->header->id << endl;
        cout << "header:" << this->header->len << endl;
        cout << "queryZone:" << this->queryZone->len << endl;
    }
}

UdpDNSResponse::~UdpDNSResponse() {
    if (header != nullptr) {
        delete header;
    }
    if (queryZone != nullptr) {
        delete queryZone;
    }
    for (auto &ptr : answerZones) {
        delete ptr;
    }
}

UdpDNSResponse::UdpDNSResponse(uint64_t len) : BasicData(len) {
}

uint32_t UdpDNSResponse::fistIP() const {
    if (this->ips.size() > 0) {
        return this->ips[0];
    }
    return 0;
}
string UdpDNSResponse::fistIPArea() const {
    uint32_t ip = this->fistIP();
    return st::areaip::getArea(ip);
}
vector<string> UdpDNSResponse::IPAreas() const {
    vector<string> areas;
    for (auto it = this->ips.begin(); it != this->ips.end(); it++) {
        areas.emplace_back(st::areaip::getArea(*it));
    }
    return areas;
}


void UdpDNSResponse::parse(uint64_t maxReadable) {
    this->markValid();
    int answerZonesSize = 0;
    if (maxReadable > DNSHeader::DEFAULT_LEN) {
        uint64_t restUnParseSize = maxReadable;
        uint8_t *dataZone = this->data;
        bool successParsedAll = false;
        while (this->isValid() && successParsedAll == false) {
            if (this->header == nullptr) {
                DNSHeader *parseHeader = DNSHeader::parse(dataZone, restUnParseSize);
                if (parseHeader == nullptr) {
                    this->markInValid();
                } else {
                    this->header = parseHeader;
                    dataZone += this->header->len;
                    restUnParseSize -= this->header->len;
                }
            } else {
                if (this->header->responseCode == 0) {
                    if (this->queryZone == nullptr) {
                        DNSQueryZone *ptr = new DNSQueryZone(dataZone, restUnParseSize, this->header->questionCount);
                        if (!ptr->isValid()) {
                            delete ptr;
                            this->markInValid();
                        } else {
                            this->queryZone = ptr;
                            dataZone += this->queryZone->len;
                            restUnParseSize -= this->queryZone->len;
                        }
                    } else {
                        if (this->answerZones.size() < this->header->answerCount) {
                            dataZone += answerZonesSize;
                            restUnParseSize -= answerZonesSize;
                            for (auto i = this->answerZones.size(); i < this->header->answerCount && this->isValid(); i++) {
                                auto *answer = new DNSResourceZone(data, maxReadable, maxReadable - restUnParseSize);
                                if (answer->isValid()) {
                                    answerZonesSize += answer->len;
                                    dataZone += answer->len;
                                    restUnParseSize -= answer->len;
                                    answerZones.emplace_back(answer);
                                    if (answer->ipv4s.size() != 0) {
                                        for (auto ip : answer->ipv4s) {
                                            this->ips.emplace_back(ip);
                                        }
                                    }
                                } else {
                                    delete answer;
                                    this->markInValid();
                                }
                            }
                        } else {
                            successParsedAll = true;
                        }
                    }
                } else {
                    successParsedAll = true;
                }
            }
        }

    } else {
        this->markInValid();
    }
    this->len = maxReadable;
    if (!this->isValid()) {
        string domain = this->queryZone != nullptr && this->queryZone->hosts.size() > 0 ? this->queryZone->hosts[0] : "";
        APMLogger::perf("st-dns-parse-failed", {{"domain", domain}}, 0);
    }
}

UdpDNSResponse::UdpDNSResponse(uint8_t *data, uint64_t len) : BasicData(data, len) {
}

UdpDNSResponse::UdpDNSResponse(UdpDnsRequest &request, DNSRecord &record) : BasicData(1024) {
    int finalLen = 0;
    vector<uint32_t> &ips = record.ips;
    auto curData = this->data;
    bool hasRecord = !ips.empty();

    //header
    this->header = DNSHeader::generateAnswer(curData, request.dnsHeader->id, request.dnsQueryZone != nullptr ? request.dnsQueryZone->querys.size() : 0, ips.size());
    curData += this->header->len;
    finalLen += this->header->len;
    //query
    if (request.dnsQueryZone != nullptr) {
        this->queryZone = request.dnsQueryZone->copy(curData);
        finalLen += this->queryZone->len;
        curData += this->queryZone->len;
    }
    //answer
    if (hasRecord) {
        uint32_t expire = (record.expireTime - time::now()) / 1000L;
        if (expire <= 0) {
            expire = 1;
        }
        if (!record.matchArea) {
            expire = 1;
        }
        expire = max(expire, (uint32_t) 1 * 10);
        expire = min(expire, (uint32_t) 10 * 60);

        for (auto it = ips.begin(); it != ips.end(); it++) {
            DNSResourceZone *pResourceZone = DNSResourceZone::generate(curData, *it, expire);
            this->answerZones.emplace_back(pResourceZone);
            curData += pResourceZone->len;
            finalLen += pResourceZone->len;
            this->ips.emplace_back(*it);
        }
    }

    this->len = finalLen;
}

void TcpDNSResponse::parse(uint64_t maxReadable) {
    if (maxReadable > 2) {
        st::utils::read(this->data, this->dataSize);
        if (this->dataSize == maxReadable - 2) {
            UdpDNSResponse *response = new UdpDNSResponse(this->data + 2, dataSize);
            response->parse(dataSize);
            if (response->isValid()) {
                this->udpDnsResponse = response;
            } else {
                this->markInValid();
                delete response;
            }
        } else {
            this->markInValid();
        }
    } else {
        this->markInValid();
    }
    this->len = maxReadable;
}

TcpDNSResponse::TcpDNSResponse(uint16_t len) : BasicData(len) {
}

TcpDNSResponse::~TcpDNSResponse() {
    if (udpDnsResponse != nullptr) {
        delete udpDnsResponse;
    }
}
