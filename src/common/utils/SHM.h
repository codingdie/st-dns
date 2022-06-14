#ifndef ST_SHM_H
#define ST_SHM_H

#include "IPUtils.h"
#include "Logger.h"
#include "AreaIPUtils.h"
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
namespace st {
    class SHM {
        typedef boost::interprocess::managed_shared_memory::segment_manager segment_manager_t;
        typedef boost::interprocess::allocator<void, segment_manager_t> void_allocator;
        typedef boost::interprocess::allocator<char, segment_manager_t> char_allocator;
        typedef boost::interprocess::basic_string<char, std::char_traits<char>, char_allocator> shm_string;
        typedef shm_string shm_map_key_type;
        typedef shm_string shm_map_value_type;
        typedef std::pair<const shm_map_key_type, shm_map_value_type> shm_pair_type;
        typedef boost::interprocess::allocator<shm_pair_type, segment_manager_t> shm_pair_allocator;
        typedef boost::interprocess::map<shm_map_key_type, shm_map_value_type, std::less<shm_map_key_type>,
                                         shm_pair_allocator>
                shm_map;

    private:
        static std::string NAME;
        static std::string LOCK_NAME;
        boost::interprocess::managed_shared_memory *segment = nullptr;
        std::atomic<bool> initSHMSuccess;
        void checkMutexStatus();

    public:
        SHM();
        ~SHM();

        static st::SHM &share();

        std::string get(const std::string &key);

        void put(const std::string &key, const std::string &value);
        void clear();

        template<typename KeyType>
        void put(KeyType key, const std::string &value) {
            put(to_string(key), value);
        }

        void putIfAbsent(const std::string &key, const std::string &value);

        uint64_t freeSize();
    };
}// namespace st

#endif