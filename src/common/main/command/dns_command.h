//
// Created by codingdie on 10/20/22.
//

#ifndef ST_DNS_COMMAND_H
#define ST_DNS_COMMAND_H
#include "st.h"
namespace st {
    namespace command {
        namespace dns {
            static vector<string> reverse_resolve(uint32_t ip) {
                vector<string> res;
                auto cm = "dns reverse resolve --ip=" + ipv4::ip_to_str(ip);
                auto result = st::console::client::command("127.0.0.1", 5757, cm, 10);
                if (result.first) {
                    res = strutils::split(result.second, ",");
                }
                return res;
            }
        }// namespace dns
    }// namespace command
}// namespace st
#endif//ST_DNS_COMMAND_H
