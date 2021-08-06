#ifndef DNSREVERSE_SHM_H
#define DNSREVERSE_SHM_H

#include "IPUtils.h"
#include "Logger.h"
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
namespace st {
    namespace utils {
        namespace dns {
            class DNSReverseSHM {
                typedef boost::interprocess::managed_shared_memory::segment_manager segment_manager_t;
                typedef boost::interprocess::allocator<void, segment_manager_t> void_allocator;
                typedef boost::interprocess::allocator<char, segment_manager_t> char_allocator;
                typedef boost::interprocess::basic_string<char, std::char_traits<char>, char_allocator> shm_string;
                typedef uint32_t shm_map_key_type;
                typedef shm_string shm_map_value_type;
                typedef std::pair<const shm_map_key_type, shm_map_value_type> shm_pair_type;
                typedef boost::interprocess::allocator<shm_pair_type, segment_manager_t> shm_pair_allocator;
                typedef boost::interprocess::map<shm_map_key_type, shm_map_value_type, std::less<shm_map_key_type>,
                                                 shm_pair_allocator>
                        shm_map;

            private:
                const std::string NAME = "ST-DNS-SHM";
                const std::string REVERSE_NAME = "REVERSE";
                bool readOnly = false;
                boost::interprocess::managed_shared_memory *segment = nullptr;
                void_allocator *alloc_inst = nullptr;
                shm_map *dnsMap = nullptr;
                bool initSHMSuccess = false;

            public:
                static DNSReverseSHM READ;

                DNSReverseSHM(bool readOnly = true);

                ~DNSReverseSHM();

                void relocate();

                std::string query(uint32_t ip) ;

                void add(uint32_t ip, const std::string host);
            
                void addOrUpdate(uint32_t ip, const std::string host);
            };


        }// namespace dns
    }    // namespace utils
}// namespace st

#endif