//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_REQUEST_H
#define ST_DNS_REQUEST_H

#include "dns_message.h"
#include <iostream>


using namespace std;
namespace st {
    namespace dns {
        namespace protocol {

            class EDNSAdditionalZone : public basic_data {

            public:
                EDNSAdditionalZone(uint32_t len);

                static EDNSAdditionalZone *generate(uint32_t ip);
            };

            class udp_request : public basic_data {
            public:
                dns_header *header = nullptr;

                dns_query_zone *query_zone = nullptr;

                std::vector<string> hosts;

                EDNSAdditionalZone *edns_zone = nullptr;

                udp_request(const vector<std::string> &hosts);


                udp_request(uint64_t len);

                udp_request();

                virtual ~udp_request();

                bool parse(uint32_t n);

                string get_host() const;

                dns_query::Type get_query_type() const;

                uint16_t get_query_type_value() const;

            protected:
                void init(uint8_t *data, const vector<std::string> &hosts);
            };

            class tcp_request : public udp_request {
            public:
                const static uint8_t SIZE_HEADER = 2;
                explicit tcp_request(const vector<std::string> &hosts);

                virtual ~tcp_request();
            };

        }// namespace protocol
    }    // namespace dns
}// namespace st
#endif//ST_DNS_REQUEST_H
