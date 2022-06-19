//
// Created by codingdie on 2020/5/22.
//

#include "response.h"
using namespace st::dns;

void udp_response::print() const {
    BasicData::print();

    if (isValid()) {
        cout << "id:" << this->header->id << endl;
        cout << "header:" << this->header->len << endl;
        cout << "queryZone:" << this->queryZone->len << endl;
    }
}

udp_response::~udp_response() {
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

udp_response::udp_response(uint64_t len) : BasicData(len) {
}

uint32_t udp_response::fist_ip() const {
    if (this->ips.size() > 0) {
        return this->ips[0];
    }
    return 0;
}
string udp_response::fist_ip_area() const {
    uint32_t ip = this->fist_ip();
    return st::areaip::manager::uniq().get_area(ip);
}
vector<string> udp_response::ip_areas() const {
    vector<string> areas;
    for (auto it = this->ips.begin(); it != this->ips.end(); it++) {
        auto area = st::areaip::manager::uniq().get_area(*it);
        if (area.compare("default") == 0) {
            logger::WARN << "ip" << st::utils::ipv4::ip_to_str(*it) << "area not recognized" << END;
        }
        areas.emplace_back(area);
    }
    return areas;
}


void udp_response::parse(uint64_t max_readable) {
    this->markValid();
    int answerZonesSize = 0;
    if (max_readable > DNSHeader::DEFAULT_LEN) {
        uint64_t restUnParseSize = max_readable;
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
                                auto *answer = new DNSResourceZone(data, max_readable, max_readable - restUnParseSize);
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
    this->len = max_readable;
    if (!this->isValid()) {
        string domain = this->queryZone != nullptr && this->queryZone->hosts.size() > 0 ? this->queryZone->hosts[0] : "";
        apm_logger::perf("st-dns-parse-failed", {{"domain", domain}}, 0);
    }
}

udp_response::udp_response(uint8_t *data, uint64_t len) : BasicData(data, len) {
}

udp_response::udp_response(udp_request &request, dns_record &record, uint32_t expire) : BasicData(1024) {
    int finalLen = 0;
    vector<uint32_t> &ips = record.ips;
    auto curData = this->data;
    bool hasRecord = !ips.empty();

    //header
    this->header = DNSHeader::generateAnswer(curData, request.header->id, request.query_zone != nullptr ? request.query_zone->querys.size() : 0, ips.size());
    curData += this->header->len;
    finalLen += this->header->len;
    //query
    if (request.query_zone != nullptr) {
        this->queryZone = request.query_zone->copy(curData);
        finalLen += this->queryZone->len;
        curData += this->queryZone->len;
    }
    //answer
    if (hasRecord) {
        for (auto it = ips.begin(); it != ips.end(); it++) {
            if (finalLen + DNSResourceZone::DEFAULT_SINGLE_IP_LEN >= DNSResourceZone::MAX_LEN) {
                break;
            }
            DNSResourceZone *pResourceZone = DNSResourceZone::generate(curData, *it, expire);
            this->answerZones.emplace_back(pResourceZone);
            curData += pResourceZone->len;
            finalLen += pResourceZone->len;
            this->ips.emplace_back(*it);
        }
    }

    this->len = finalLen;
}

void tcp_response::parse(uint64_t maxReadable) {
    if (maxReadable > 2) {
        st::utils::read(this->data, this->data_size);
        if (this->data_size == maxReadable - 2) {
            st::dns::udp_response *response = new st::dns::udp_response(this->data + 2, data_size);
            response->parse(data_size);
            if (response->isValid()) {
                this->response = response;
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

tcp_response::tcp_response(uint16_t len) : BasicData(len) {
}

tcp_response::~tcp_response() {
    if (response != nullptr) {
        delete response;
    }
}
