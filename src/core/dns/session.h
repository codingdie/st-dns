//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_SESSION_H
#define ST_DNS_SESSION_H

#include "protocol/protocol.h"
#include "config.h"
#include "utils/utils.h"
#include <boost/asio.hpp>
namespace st {
    namespace dns {

        class session {

        private:
            uint64_t id;
            uint64_t time = 0;

        public:
            enum process_type {
                QUERY = 1,
                FORWARD = 2,
                DROP = 3
            };
            apm_logger logger;
            boost::asio::ip::udp::endpoint client_endpoint;
            protocol::udp_request request;
            dns_record record;
            process_type process_type = QUERY;
            protocol::udp_response *response = nullptr;
            vector<remote_dns_server *> servers;

            session(uint64_t id);

            virtual ~session();


            uint64_t get_id() const;

            string get_host() const;

            protocol::dns_query::Type get_query_type() const;

            uint16_t get_query_type_value() const;

            void set_time(uint64_t time);

            uint64_t get_time() const;
        };

    }// namespace dns
}// namespace st


#endif//ST_DNS_SESSION_H
