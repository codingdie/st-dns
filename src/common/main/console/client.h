//
// Created by codingdie on 10/13/22.
//

#ifndef ST_DNS_CLIENT_H
#define ST_DNS_CLIENT_H
#include "st.h"
#include <functional>
using namespace st::utils;
namespace st {
    namespace console {
        namespace client {
            static pair<bool, std::string> command(const std::string &ip, uint16_t port, const std::string &command,
                                                   uint32_t timeout,
                                                   const std::function<bool()> &should_cancel = std::function<bool()>()) {
                try {
                    io_context ic;
                    udp::socket socket(ic, udp::v4());
                    deadline_timer timer(ic);
                    deadline_timer cancel_timer(ic);
                    std::function<void()> schedule_cancel_check;
                    if (should_cancel) {
                        schedule_cancel_check = [&]() {
                            cancel_timer.expires_from_now(boost::posix_time::milliseconds(10));
                            cancel_timer.async_wait([&](boost::system::error_code ec) {
                                if (ec == boost::asio::error::operation_aborted) {
                                    return;
                                }
                                if (should_cancel()) {
                                    boost::system::error_code ignored_ec;
                                    timer.cancel(ignored_ec);
                                    socket.cancel(ignored_ec);
                                    socket.close(ignored_ec);
                                    return;
                                }
                                schedule_cancel_check();
                            });
                        };
                        schedule_cancel_check();
                    }
                    timer.expires_from_now(boost::posix_time::milliseconds(timeout));
                    timer.async_wait([&](boost::system::error_code ec) {
                        if (ec == boost::asio::error::operation_aborted) {
                            return;
                        }
                        boost::system::error_code ignored_ec;
                        cancel_timer.cancel(ignored_ec);
                        socket.cancel(ignored_ec);
                        socket.close(ignored_ec);
                    });
                    char res[10240] = "";
                    udp::endpoint endpoint(make_address_v4(ip), port);
                    socket.async_send_to(buffer(command.c_str(), command.size()), endpoint,
                                         [&](boost::system::error_code error, size_t size) {
                                             if (!error) {
                                                 socket.async_receive_from(
                                                         buffer(res, sizeof(char) * 10240), endpoint,
                                                         [&](boost::system::error_code error, size_t size) {
                                                             res[size + 1] = '\0';
                                                             boost::system::error_code ignored_ec;
                                                             timer.cancel(ignored_ec);
                                                             cancel_timer.cancel(ignored_ec);
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
                    }
                } catch (boost::system::system_error &ex) {
                    logger::ERROR << "udp command error!" << ex.code().message() << ex.what() << logger::ENDL;
                }
                return make_pair(false, "network error!");
            }
        };// namespace client

    }// namespace console
}// namespace st

#endif//ST_DNS_CLIENT_H
