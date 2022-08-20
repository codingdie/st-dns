#ifndef ST_SHM_KV_H
#define ST_SHM_KV_H


#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace st {
    namespace shm {
        class kv {
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
            static std::unordered_map<std::string, st::shm::kv *> INSTANCES;
            static std::mutex create_mutex;

            boost::interprocess::managed_shared_memory *segment = nullptr;
            std::string ns;
            void check_mutex_status();
            std::string shm_name();
            std::string mutex_name();

        public:
            kv(std::string ns, uint32_t max_size);

            ~kv();

            static st::shm::kv *create(const std::string &ns, uint32_t max_size);

            static st::shm::kv *share(const std::string &ns);

            std::string get(const std::string &key);

            void put(const std::string &key, const std::string &value);
            void erase(const std::string &key);

            void clear();

            template<typename KeyType>
            void put(KeyType key, const std::string &value) {
                put(std::to_string(key), value);
            }

            template<typename ValueType>
            void put(const std::string &key, ValueType value) {
                put(key, std::to_string(value));
            }

            void put_if_absent(const std::string &key, const std::string &value);

            uint64_t free_size();

            static uint64_t free_size(const std::string &ns);
        };
    }// namespace shm

}// namespace st

#endif