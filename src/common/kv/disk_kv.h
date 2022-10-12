//
// Created by codingdie on 10/12/22.
//

#ifndef ST_DNS_DISK_KV_H
#define ST_DNS_DISK_KV_H
#include "kv.h"
#include "utils/file.h"
#include <leveldb/db.h>
namespace st {
    namespace kv {

        class disk_kv : public abstract_kv {

        public:
            disk_kv(const std::string &ns, uint32_t max_size);
            ~disk_kv() override;
            std::string get(const std::string &key) override;
            void put(const std::string &key, const std::string &value) override;
            void put(const std::string &key, const std::string &value, uint32_t expire) override;
            void erase(const std::string &key) override;
            void clear() override;

        private:
            leveldb::DB *db = nullptr;
            leveldb::Options options;
        };

    }// namespace kv
}// namespace st

#endif//ST_DNS_DISK_KV_H
