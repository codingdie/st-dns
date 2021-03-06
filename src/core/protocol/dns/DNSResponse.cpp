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

void UdpDNSResponse::parse(uint64_t maxReadable) {
    this->markValid();
    if (maxReadable > DNSHeader::DEFAULT_LEN) {
        uint64_t restUnParseSize = maxReadable;
        uint8_t *dataZone = this->data;
        bool successParsedAll = false;
        while (this->isValid() && successParsedAll == false) {
            if (this->header == nullptr) {
                DNSHeader *parseHeader = new DNSHeader(dataZone, restUnParseSize);
                if (!parseHeader->isValid()) {
                    delete parseHeader;
                    this->markInValid();
                } else {
                    this->header = parseHeader;
                    ;
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
                            dataZone += this->answerZonesSize;
                            restUnParseSize -= this->answerZonesSize;
                            for (auto i = this->answerZones.size(); i < this->header->answerCount && this->isValid(); i++) {
                                auto *answer = new DNSResourceZone(data, maxReadable, maxReadable - restUnParseSize);
                                if (answer->isValid()) {
                                    this->answerZonesSize += answer->len;
                                    dataZone += answer->len;
                                    restUnParseSize -= answer->len;
                                    answerZones.emplace_back(answer);
                                    if (answer->ipv4s.size() != 0) {
                                        for (auto ip : answer->ipv4s) {
                                            this->ips.emplace(ip);
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
}

UdpDNSResponse::UdpDNSResponse(uint8_t *data, uint64_t len) : BasicData(data, len) {
}

UdpDNSResponse::UdpDNSResponse(UdpDnsRequest &request, DNSRecord &record) {
    unordered_set<uint32_t> &ips = record.ips;
    bool hasRecord = !ips.empty();
    this->header = DNSHeader::generateAnswer(request.dnsHeader->id, request.dnsQueryZone != nullptr ? request.dnsQueryZone->querys.size() : 0, ips.size());
    if (hasRecord) {
        uint32_t expire = (record.expireTime - time::now()) / 1000L;
        if (expire <= 0) {
            expire = 1;
        }
        if (!record.matchArea) {
            expire = 1;
        }
        expire = max(expire, (uint32_t) 1 * 60);
        for (auto it = ips.begin(); it != ips.end(); it++) {
            DNSResourceZone *pResourceZone = DNSResourceZone::generate(*it, expire);
            this->answerZones.emplace_back(pResourceZone);
            this->answerZonesSize += pResourceZone->len;
            this->ips.emplace(*it);
        }
    }

    this->len = this->header->len + this->answerZonesSize;
    if (request.dnsQueryZone != nullptr) {
        this->queryZone = request.dnsQueryZone->copy();
        this->len += this->queryZone->len;
    }

    this->alloc(len);
    auto ptr = this->data;
    st::utils::copy(this->header->data, ptr, this->header->len);
    ptr += this->header->len;
    if (request.dnsQueryZone != nullptr) {
        st::utils::copy(this->queryZone->data, ptr, this->queryZone->len);
        ptr += this->queryZone->len;
    }
    if (hasRecord) {
        for (auto it = this->answerZones.begin(); it < this->answerZones.end(); it++) {
            DNSResourceZone *pZone = *it.base();
            st::utils::copy(pZone->data, ptr, pZone->len);
            ptr += pZone->len;
        }
    }
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
