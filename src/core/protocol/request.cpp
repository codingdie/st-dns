//
// Created by codingdie on 2020/5/21.
//

#include "request.h"
#include <chrono>
#include <vector>
using namespace st::dns::protocol;
void udp_request::init(uint8_t *data, const vector<std::string> &hosts) {
    this->header = dns_header::generateQuery(data, hosts.size());
    this->query_zone = dns_query_zone::generate(data + this->header->len, hosts);
    this->hosts = hosts;
    this->len = this->header->len + this->query_zone->len;
}
udp_request::udp_request(const vector<std::string> &hosts) : basic_data(1024) {
    this->init(this->data, hosts);
}
udp_request::~udp_request() {
    if (header != nullptr) {
        delete header;
    }
    if (query_zone != nullptr) {
        delete query_zone;
    }
}


bool udp_request::parse(uint32_t n) {
    if (n > dns_header::DEFAULT_LEN) {
        dns_header *header = dns_header::parse(data, dns_header::DEFAULT_LEN);
        if (header != nullptr) {
            this->header = header;
            dns_query_zone *queryZone = new dns_query_zone(data + dns_header::DEFAULT_LEN, n - dns_header::DEFAULT_LEN, header->questionCount);
            if (queryZone->is_valid()) {

                if (queryZone->querys.size() == 0) {
                    this->print_hex();
                    mark_invalid();
                } else {
                    query_zone = queryZone;
                    for (auto it : queryZone->querys) {
                        if (!it->domain->domain.empty()) {
                            this->hosts.emplace_back(it->domain->domain);
                        }
                    }
                }
            } else {
                mark_invalid();
            }
        } else {
            mark_invalid();
        }
    } else {
        mark_invalid();
    }
    this->len = n;
    return is_valid();
}


udp_request::udp_request(uint64_t len) : basic_data(len) {
}

string udp_request::get_host() const {
    if (hosts.empty()) {
        return "";
    }
    return hosts.front();
}
dns_query::Type udp_request::get_query_type() const {
    return this->query_zone->querys[0]->queryType;
}

uint16_t udp_request::get_query_type_value() const {
    return this->query_zone->querys[0]->queryTypeValue;
}

udp_request::udp_request() {
}

tcp_request::tcp_request(const vector<std::string> &hosts) : udp_request(1024 + SIZE_HEADER) {
    this->init(this->data + 2, hosts);

    uint8_t len_arr[2];
    uint16_t dataLen = this->len;
    st::utils::to_bytes(len_arr, dataLen);
    st::utils::copy(len_arr, data, 0U, 0, SIZE_HEADER);
    this->len = this->len + SIZE_HEADER;
}

tcp_request::~tcp_request() {
    if (edns_zone != nullptr) {
        delete edns_zone;
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

EDNSAdditionalZone::EDNSAdditionalZone(uint32_t len) : basic_data(len) {
}
