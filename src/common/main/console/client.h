//
// Created by codingdie on 10/13/22.
//

#ifndef ST_DNS_CLIENT_H
#define ST_DNS_CLIENT_H
#include "st.h"
namespace st {
    namespace console {
        namespace client {
            static pair<bool, std::string> command(const std::string &ip, uint16_t port, const std::string &command, uint32_t timeout) {
                io_context ic;
                udp::socket socket(ic, udp::endpoint(udp::v4(), 0));
                deadline_timer timer(ic);
                timer.expires_from_now(boost::posix_time::milliseconds(timeout));
                timer.async_wait([&](boost::system::error_code ec) {
                    socket.cancel();
                });
                char res[10240] = "";
                udp::endpoint endpoint(make_address_v4(ip), port);
                socket.async_send_to(
                        buffer(command.c_str(), command.size()),
                        endpoint,
                        [&](boost::system::error_code error, size_t size) {
                            if (!error) {
                                socket.async_receive_from(
                                        buffer(res, sizeof(char) * 10240),
                                        endpoint,
                                        [&](boost::system::error_code error, size_t size) {
                                            res[size + 1] = '\0';
                                            timer.cancel();
                                        });
                            }
                        });
                ic.run();
                string result = res;
                auto splits = utils::strutils::split(result, "\n");
                if (!splits.empty()) {
                    if (splits[0] == "success") {
                        return make_pair(true, utils::strutils::trim(result.substr(7)));
                    } else {
                        return make_pair(false, utils::strutils::trim(result.substr(6)));
                    }
                } else {
                    return make_pair(false, "network error!");
                }
            }
        };// namespace client

    }// namespace console
}// namespace st

#endif//ST_DNS_CLIENT_H
