//
// Created by System Administrator on 2020/10/11.
//

#ifndef ST_DNS_TLIOCONTEXT_H
#define ST_DNS_TLIOCONTEXT_H

#include <boost/asio.hpp>

namespace st {
    namespace utils {
        namespace asio {

            using namespace boost::asio;

            class TLIOContext {
            public:
                static io_context &getIOContext();

            };

        }
    }
}

#endif //ST_DNS_TLIOCONTEXT_H
