//
// Created by codingdie on 2020/5/22.
//

#include "response.h"
using namespace st::dns::protocol;

void udp_response::print() const {
    basic_data::print();

    if (is_valid()) {
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

udp_response::udp_response(uint64_t len) : basic_data(len) {
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
    this->mark_valid();
    int answerZonesSize = 0;
    if (max_readable > dns_header::DEFAULT_LEN) {
        uint64_t restUnParseSize = max_readable;
        uint8_t *dataZone = this->data;
        bool successParsedAll = false;
        while (this->is_valid() && successParsedAll == false) {
            if (this->header == nullptr) {
                dns_header *parseHeader = dns_header::parse(dataZone, restUnParseSize);
                if (parseHeader == nullptr) {
                    this->mark_invalid();
                } else {
                    this->header = parseHeader;
                    dataZone += this->header->len;
                    restUnParseSize -= this->header->len;
                }
            } else {
                if (this->header->responseCode == 0) {
                    if (this->queryZone == nullptr) {
                        dns_query_zone *ptr = new dns_query_zone(dataZone, restUnParseSize, this->header->questionCount);
                        if (!ptr->is_valid()) {
                            delete ptr;
                            this->mark_invalid();
                        } else {
                            this->queryZone = ptr;
                            dataZone += this->queryZone->len;
                            restUnParseSize -= this->queryZone->len;
                        }
                    } else {
                        if (this->answerZones.size() < this->header->answerCount) {
                            dataZone += answerZonesSize;
                            restUnParseSize -= answerZonesSize;
                            for (auto i = this->answerZones.size(); i < this->header->answerCount && this->is_valid(); i++) {
                                auto *answer = new dns_resource_zone(data, max_readable, max_readable - restUnParseSize);
                                if (answer->is_valid()) {
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
                                    this->mark_invalid();
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
        this->mark_invalid();
    }
    this->len = max_readable;
    if (!this->is_valid()) {
        string domain = this->queryZone != nullptr && this->queryZone->hosts.size() > 0 ? this->queryZone->hosts[0] : "";
        apm_logger::perf("st-dns-parse-failed", {{"domain", domain}}, 0);
    }
}

udp_response::udp_response(uint8_t *data, uint64_t len) : basic_data(data, len) {
}

udp_response::udp_response(udp_request &request, dns_record &record, uint32_t expire) : basic_data(1024) {
    int finalLen = 0;
    vector<uint32_t> ips = record.ips;
    logger::DEBUG << "udp_response construct: record.ips.size=" << record.ips.size() << END;
    if (ips.size() >= 5) {
        ips.assign(record.ips.begin(), record.ips.begin() + 5);
    }

    auto curData = this->data;
    bool hasRecord = !ips.empty();
    logger::DEBUG << "udp_response construct: ips.size=" << ips.size() << "hasRecord=" << hasRecord << END;

    //header
    this->header = dns_header::generateAnswer(curData, request.header->id, request.query_zone != nullptr ? request.query_zone->querys.size() : 0, ips.size());
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
        logger::DEBUG << "udp_response construct: generating answers, ips.size=" << ips.size() << END;
        for (auto it = ips.begin(); it != ips.end(); it++) {
            if (finalLen + dns_resource_zone::DEFAULT_SINGLE_IP_LEN >= dns_resource_zone::MAX_LEN) {
                logger::WARN << "udp_response construct: answer size limit reached" << END;
                break;
            }
            dns_resource_zone *pResourceZone = dns_resource_zone::generate(curData, *it, expire);
            this->answerZones.emplace_back(pResourceZone);
            curData += pResourceZone->len;
            finalLen += pResourceZone->len;
            this->ips.emplace_back(*it);
            logger::DEBUG << "udp_response construct: added answer, ip=" << st::utils::ipv4::ip_to_str(*it)
                         << "finalLen=" << finalLen << END;
        }
    } else {
        logger::WARN << "udp_response construct: hasRecord=false, no answers generated" << END;
    }

    this->len = finalLen;
    logger::DEBUG << "udp_response construct: final len=" << finalLen << "answerZones.size=" << this->answerZones.size() << END;
}

void tcp_response::parse(uint64_t maxReadable) {
    if (maxReadable > 2) {
        st::utils::read(this->data, this->data_size);
        if (this->data_size == maxReadable - 2) {
            udp_response *response = new udp_response(this->data + 2, data_size);
            response->parse(data_size);
            if (response->is_valid()) {
                this->response = response;
            } else {
                this->mark_invalid();
                delete response;
            }
        } else {
            this->mark_invalid();
        }
    } else {
        this->mark_invalid();
    }
    this->len = maxReadable;
}

tcp_response::tcp_response(uint16_t len) : basic_data(len) {
}

tcp_response::~tcp_response() {
    if (response != nullptr) {
        delete response;
    }
}
