//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_RESPONSE_H
#define ST_DNS_RESPONSE_H


#include "DNSMessage.h"
#include "request.h"
#include "dns_cache.h"
#include <iostream>
using namespace std;
namespace st {
    namespace dns {

        class udp_response : public BasicData {
        public:
            DNSHeader *header = nullptr;
            DNSQueryZone *queryZone = nullptr;
            vector<DNSResourceZone *> answerZones;
            vector<uint32_t> ips;

            udp_response(udp_request &request, dns_record &record, uint32_t expire);

            udp_response(uint8_t *data, uint64_t len);

            udp_response(uint64_t len);

            virtual ~udp_response();

            void parse(uint64_t max_readable);

            void print() const override;

            uint32_t fist_ip() const;
            string fist_ip_area() const;
            vector<string> ip_areas() const;
        };


        class tcp_response : public BasicData {

        public:
            uint16_t data_size = 0;
            st::dns::udp_response *response = nullptr;

            virtual ~tcp_response();

            tcp_response(uint16_t len);

            void parse(uint64_t maxReadable);
        };

    }// namespace dns
}// namespace st
#endif//ST_DNS_RESPONSE_H
