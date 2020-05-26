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
    for (auto ptr1:answerZones) {
        delete ptr1;
    }
}

UdpDNSResponse::UdpDNSResponse(uint64_t len) : BasicData(len) {}

void UdpDNSResponse::parse(uint64_t maxReadable) {
    this->markValid();
    if (maxReadable > DNSHeader::DEFAULT_LEN) {
        uint64_t restUnParseSize = maxReadable;
        byte *dataZone = this->data;
        bool successParsedAll = false;
        while (this->isValid() && successParsedAll == false) {
            if (this->header == nullptr) {
                DNSHeader *parseHeader = new DNSHeader(dataZone, restUnParseSize);
                if (!parseHeader->isValid()) {
                    delete parseHeader;
                    this->markInValid();
                } else {
                    this->header = parseHeader;;
                }
            } else {
                dataZone += this->header->len;
                restUnParseSize -= this->header->len;
                if (this->header->responseCode == 0) {
                    if (this->queryZone == nullptr) {
                        DNSQueryZone *ptr = new DNSQueryZone(dataZone, restUnParseSize, this->header->questionCount);
                        if (!ptr->isValid()) {
                            delete ptr;
                            this->markInValid();
                        } else {
                            this->queryZone = ptr;
                        }
                    } else {
                        dataZone += this->queryZone->len;
                        restUnParseSize -= this->queryZone->len;
                        dataZone += this->answerZonesSize;
                        restUnParseSize -= this->answerZonesSize;
                        if (this->answerZones.size() < this->header->answerCount) {
                            for (auto i = this->answerZones.size();
                                 i < this->header->answerCount && this->isValid(); i++) {
                                auto *answer = new DNSResourceZone(data, maxReadable, maxReadable - restUnParseSize);
                                if (answer->isValid()) {
                                    this->answerZonesSize += answer->len;
                                    dataZone += answer->len;
                                    restUnParseSize -= answer->len;
                                    answerZones.emplace_back(answer);
                                    if (answer->ipv4 != 0) {
                                        this->ips.emplace_back(answer->ipv4);
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

UdpDNSResponse::UdpDNSResponse(byte *data, uint64_t len) : BasicData(data, len) {

}

void TcpDNSResponse::parse(uint64_t maxReadable) {
    if (maxReadable > 2) {
        read(this->data, this->dataSize);
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
