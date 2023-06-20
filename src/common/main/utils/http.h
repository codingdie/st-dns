//
// Created by codingdie on 6/15/23.
//

#ifndef ST_DNS_HTTP_H
#define ST_DNS_HTTP_H

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif

#include "thirdpart/httplib.h"
#include <map>
#include <utility>

namespace st {
    namespace utils {
        namespace http {
            inline std::pair<int, std::string> get(const std::string &host, const std::string &path, const httplib::Headers &headers) {
                try {
                    httplib::Client cli(host);
                    cli.enable_server_certificate_verification(false);
                    cli.set_follow_location(true);
                    auto result = cli.Get(path, headers);
                    if (result.error() == httplib::Error::Success) {
                        return std::make_pair(result->status, result->body);
                    } else {
                        return std::make_pair(0, "");
                    }
                } catch (std::exception const &e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
                return std::make_pair(0, "");
            }
        }// namespace http
    }    // namespace utils
}// namespace st

#endif//ST_DNS_HTTP_H
