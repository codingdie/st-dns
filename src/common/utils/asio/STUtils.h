//
// Created by codingdie on 2020/10/7.
//

#ifndef ST_PROXY_ASIO_UTILS_H
#define ST_PROXY_ASIO_UTILS_H

#include <boost/asio.hpp>
using namespace boost::asio::ip;
using namespace std;
using namespace boost::asio;


namespace st {
    namespace utils {
        namespace asio {
            inline string addrStr(tcp::endpoint &endpoint) {
                return endpoint.address().to_string() + ":" + to_string(endpoint.port());
            }
        }// namespace asio
    }    // namespace utils
}// namespace st

#endif//ST_PROXY_ASIO_UTILS_H
