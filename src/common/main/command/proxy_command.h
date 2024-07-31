//
// Created by codingdie on 10/20/22.
//

#ifndef ST_PROXY_COMMAND_H
#define ST_PROXY_COMMAND_H
#include "st.h"
namespace st {
    namespace command {
        namespace proxy {

            static uint16_t register_area_port(const string& ip, uint16_t port, const string &area) {
                auto begin = time::now();
                auto cm = "proxy register area virtual port --area=" + area + " --port=" + to_string(port) +
                          " --ip=" + ip;
                auto result = st::console::client::command("127.0.0.1", 5858, cm, 100);
                apm_logger::perf("register-area-port", {}, st::utils::time::now() - begin);
                if (result.first) {
                    return stoi(result.second);
                } else {
                    return 0;
                }
            }
            static vector<string> get_ip_available_proxy_areas(const string& ip) {
                auto begin = time::now();
                auto cm = "proxy ip available areas --ip=" + ip;
                auto result = st::console::client::command("127.0.0.1", 5858, cm, 1000);
                vector<string> areas;
                apm_logger::perf("get-ip-available-proxy-areas", {}, st::utils::time::now() - begin);
                if (result.first) {
                    areas = st::utils::strutils::split(result.second, ",");
                } else {
                    logger::ERROR << "get_ip_available_proxy_areas error! ip:" << ip << "msg" << result.second << END;
                }
                return areas;
            }
        }// namespace proxy
    }// namespace command
}// namespace st
#endif//ST_PROXY_COMMAND_H
