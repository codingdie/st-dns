#ifndef ST_DNS_SHM_H
#define ST_DNS_SHM_H

#include "SHM.h"
namespace st {
    namespace dns {
        class SHM {
        private:
            uint16_t calVirtualPort(const std::string &area);

        public:
            static st::dns::SHM &share();

            std::string reverse(uint32_t ip);

            uint16_t setVirtualPort(uint32_t ip, uint16_t port, const std::string &area);

            std::pair<std::string, uint16_t> getRealPort(uint32_t ip, uint16_t port);
        };


    }// namespace dns
}// namespace st

#endif