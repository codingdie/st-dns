//
// Created by codingdie on 10/12/22.
//

#include "disk_kv.h"
#include <leveldb/cache.h>
#include "proto/kv.pb.h"
#include "utils//base64.h"
static const char *const KV_FOLDER = "/var/lib/st/kv/";
namespace st {
    namespace kv {
        disk_kv::~disk_kv() {
            delete db;
        }
        std::string disk_kv::get(const std::string &key) {
            string data;
            leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &data);
            proto::value val;
            if (s.ok()) {
                val.ParseFromString(data);
                if (not_expired(val)) {
                    return utils::base64::decode(val.str());
                }
                this->erase(key);
            }
            return "";
        }
        bool disk_kv::not_expired(const proto::value &val) { return val.expire() == 0 || val.expire() < utils::time::now() / 1000; }
        void disk_kv::put(const std::string &key, const std::string &value) {
            this->put(key, value, 0);
        }

        void disk_kv::put(const std::string &key, const std::string &value, uint32_t expire) {
            proto::value val;
            val.set_str(utils::base64::encode(value));
            if (expire != 0) {
                val.set_expire(utils::time::now() / 1000 + expire);
            } else {
                val.set_expire(expire);
            }
            db->Put(leveldb::WriteOptions(), key, val.SerializeAsString());
        }
        void disk_kv::erase(const std::string &key) {
            db->Delete(leveldb::WriteOptions(), key);
        }
        void disk_kv::clear() {
            leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                erase(it->key().ToString());
            }
            delete it;
        }
        disk_kv::disk_kv(const std::string &ns, uint32_t max_size) : abstract_kv(ns, max_size) {
            options.create_if_missing = true;
            st::utils::file::mkdirs(KV_FOLDER);
            leveldb::Status status = leveldb::DB::Open(options, KV_FOLDER + ns, &db);
            options.block_cache = leveldb::NewLRUCache(max_size);
            assert(status.ok());
        }
        void disk_kv::list(std::function<void(const std::string &, const std::string &)> consumer) {
            leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                proto::value val;
                val.ParseFromString(it->value().ToString());
                if (not_expired(val)) {
                    consumer(it->key().ToString(), utils::base64::decode(val.str()));
                }
            }
            delete it;
        }
    }// namespace kv
}// namespace st