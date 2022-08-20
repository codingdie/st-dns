//
// Created by codingdie on 7/13/22.
//

#include "proxy_shm.h"
#include "utils/moment.h"
namespace st {
    namespace proxy {
        shm &shm::uniq() {
            static shm in;
            return in;
        }
        void shm::forbid_ip(uint32_t ip) { ip_blacklist->put(ip, std::to_string(utils::time::now())); }
        void shm::recover_ip(uint32_t ip) { ip_blacklist->erase(std::to_string(ip)); }
        bool shm::is_ip_forbid(uint32_t ip) {
            auto str = ip_blacklist->get(std::to_string(ip));
            if (str.empty()) {
                return false;
            }
            uint64_t time = std::stoull(str);
            return utils::time::now() - time < IP_FORBID_TIME;
        }
    }// namespace proxy
}// namespace st