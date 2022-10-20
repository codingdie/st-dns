//
// Created by codingdie on 7/13/22.
//

#ifndef ST_PROXY_PROXY_SHM_H
#define ST_PROXY_PROXY_SHM_H


#include "kv/shm_kv.h"
#include <vector>

namespace st {
    namespace proxy {

        class shm {
        private:

        public:
            void forbid_ip(uint32_t ip);
            void recover_ip(uint32_t ip);
            bool is_ip_forbid(uint32_t ip);
            std::vector<std::string> forbid_ip_list();
            static shm &uniq();
            static const uint32_t IP_FORBID_TIME = 60L * 10L * 1000L;
        };

    }// namespace proxy
}// namespace st

#endif//ST_PROXY_PROXY_SHM_H
