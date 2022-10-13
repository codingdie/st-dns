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
#include "abstract_kv.h"
namespace st {
    namespace kv {
        class shm_kv : public st::kv::abstract_kv {
        public:
            shm_kv(const std::string &ns, uint32_t maxSize);
            ~shm_kv() override;
            std::string get(const std::string &key) override;
            void put(const std::string &key, const std::string &value) override;
            void put(const std::string &key, const std::string &value, uint32_t expire) override;
            void erase(const std::string &key) override;
            void clear() override;

            void put_if_absent(const std::string &key, const std::string &value);
            static kv::shm_kv *create(const std::string &ns, uint32_t max_size);
            static kv::shm_kv *share(const std::string &ns);
            uint64_t free_size();

        private:
            static std::string NAME;
            static std::string LOCK_NAME;
            static std::unordered_map<std::string, kv::shm_kv *> INSTANCES;
            static std::mutex create_mutex;
            boost::interprocess::managed_shared_memory *segment = nullptr;
            void check_mutex_status();
            std::string shm_name();
            std::string mutex_name();
        };
    }// namespace kv
}// namespace st

#endif