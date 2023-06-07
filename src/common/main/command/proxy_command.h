//
// Created by codingdie on 10/20/22.
//

#ifndef ST_PROXY_COMMAND_H
#define ST_PROXY_COMMAND_H
#include "st.h"
namespace st {
    namespace command {
        namespace proxy {

            static uint16_t register_area_port(uint32_t ip, uint16_t port, const string &area) {
                auto begin = time::now();
                auto cm = "proxy register area virtual port --area=" + area + " --port=" + to_string(port) +
                          " --ip=" + ipv4::ip_to_str(ip);
                auto result = st::console::client::command("127.0.0.1", 5858, cm, 10);
                apm_logger::perf("register-area-port", {}, st::utils::time::now() - begin);
                if (result.first) {
                    return stoi(result.second);
                } else {
                    return 0;
                }
            }
        }// namespace proxy
    }    // namespace command
}// namespace st
#endif//ST_PROXY_COMMAND_H
