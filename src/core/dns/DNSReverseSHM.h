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
                DNSReverseSHM(bool readOnly = true) : readOnly(readOnly) {
                    try {
                        if (!readOnly) {
                            boost::interprocess::shared_memory_object::remove(NAME.c_str());
                            segment = new boost::interprocess::managed_shared_memory(boost::interprocess::create_only,
                                                                                     NAME.c_str(), 1024 * 1024 * 4);
                            alloc_inst = new void_allocator(segment->get_segment_manager());
                            dnsMap = segment->construct<shm_map>(REVERSE_NAME.c_str())(std::less<shm_map_key_type>(),
                                                                                       *alloc_inst);
                            initSHMSuccess = true;
                        } else {
                            relocate();
                        }
                    } catch (const std::exception &e) {
                        Logger::ERROR << "init DNSReverseSHM error!" << e.what() << END;
                    }
                }
                ~DNSReverseSHM() {
                    if (!readOnly) {
                        if (segment != nullptr) {
                            segment->destroy<shm_map>(REVERSE_NAME.c_str());
                        }
                        boost::interprocess::shared_memory_object::remove(NAME.c_str());
                        if (alloc_inst != nullptr) {
                            delete alloc_inst;
                        }
                    }
                    if (segment != nullptr) {
                        delete segment;
                    }
                }
                void relocate() {
                    try {
                        if (segment != nullptr) {
                            delete segment;
                        }
                        segment = new boost::interprocess::managed_shared_memory(boost::interprocess::open_only,
                                                                                 NAME.c_str());
                        dnsMap = segment->find<shm_map>(REVERSE_NAME.c_str()).first;
                        initSHMSuccess = true;
                    } catch (const std::exception &e) {
                        Logger::ERROR << "relocate error!" << e.what() << END;
                        initSHMSuccess = false;
                    }
                }
                std::string query(uint32_t ip) {
                    if (!initSHMSuccess) {
                        return st::utils::ipv4::ipToStr(ip);
                    }
                    auto it = dnsMap->find(ip);
                    if (it != dnsMap->end()) {
                        return it->second.c_str();
                    }
                    return st::utils::ipv4::ipToStr(ip);
                }

                void add(uint32_t ip, const std::string host) {
                    if (!initSHMSuccess || readOnly || host.empty()) {
                        return;
                    }
                    shm_map_value_type shm_host(host.c_str(), *alloc_inst);
                    shm_pair_type value(ip, shm_host);
                    dnsMap->insert(value);
                }
                
                void addOrUpdate(uint32_t ip, const std::string host) {
                    if (!initSHMSuccess || readOnly || host.empty()) {
                        return;
                    }
                    shm_map_value_type shm_host(host.c_str(), *alloc_inst);
                    auto it = dnsMap->find(ip);
                    if (it != dnsMap->end()) {
                        it->second = shm_host;
                    } else {
                        shm_pair_type value(ip, shm_host);
                        dnsMap->insert(value);
                    }
                }
            };


        }// namespace dns
    }    // namespace utils
}// namespace st

#endif