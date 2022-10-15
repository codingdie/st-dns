//
// Created by codingdie on 10/12/22.
//

#ifndef ST_DNS_DISK_KV_H
#define ST_DNS_DISK_KV_H
#include "kv.h"
#include "proto/kv.pb.h"
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
            void list(std::function<void(const std::string &, const std::string &)> consumer) override;

        private:
            leveldb::DB *db = nullptr;
            leveldb::Options options;
            static bool not_expired(const st::kv::proto::value &val);
        };

    }// namespace kv
}// namespace st

#endif//ST_DNS_DISK_KV_H
