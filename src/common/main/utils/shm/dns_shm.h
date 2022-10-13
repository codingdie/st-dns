#ifndef ST_DNS_SHM_H
#define ST_DNS_SHM_H

#include "kv/shm_kv.h"
#include "utils/ipv4.h"
#include "utils/logger.h"
#include "utils/area_ip.h"
namespace st {
    namespace dns {
        class shm {
        private:
            uint16_t cal_virtual_port(const std::string &area);
            kv::shm_kv *reverse = kv::shm_kv::create("DNS-REVERSE", 4 * 1024 * 1024);
            kv::shm_kv *virtual_port = kv::shm_kv::create("DNS-VIRTUAL-PROT", 1024);

        public:
            static st::dns::shm &share();

            void add_reverse_record(uint32_t ip, std::string domain, bool overwrite);

            std::string reverse_resolve(uint32_t ip);

            uint16_t set_virtual_port(uint32_t ip, uint16_t port, const std::string &area);

            std::pair<std::string, uint16_t> get_real_port(uint32_t ip, uint16_t port);

            uint64_t free_size();
        };


    }// namespace dns
}// namespace st

#endif