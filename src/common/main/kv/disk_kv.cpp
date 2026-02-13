//
// Created by codingdie on 10/12/22.
//

#include "disk_kv.h"
#include "kv.pb.h"
#include "utils/base64.h"
#include "utils/logger.h"
#include "utils/file.h"
#include <leveldb/cache.h>
#include <unistd.h>
#include <sys/stat.h>

// 自动选择可写的目录
static std::string get_kv_folder() {
    std::string default_folder = "/var/lib/st/kv/";
    std::string tmp_folder = "/tmp/st-kv/";

    // 尝试创建默认目录
    struct stat st;
    if (stat(default_folder.c_str(), &st) == 0 || mkdir(default_folder.c_str(), 0755) == 0) {
        // 检查是否可写
        if (access(default_folder.c_str(), W_OK) == 0) {
            return default_folder;
        }
    }

    // 使用 /tmp 目录
    st::utils::file::mkdirs(tmp_folder);
    return tmp_folder;
}

static std::string KV_FOLDER = get_kv_folder();
using namespace st::utils;
namespace st {
    namespace kv {
        disk_kv::~disk_kv() {
            delete db;
            delete options.block_cache;
        }
        std::string disk_kv::get(const std::string &key) {
            auto begin = time::now();
            string data;
            bool success = db->Get(leveldb::ReadOptions(), key, &data).ok();
            proto::value val;
            string result = "";
            if (success) {
                val.ParseFromString(data);
                if (not_expired(val)) {
                    result = val.data();
                } else {
                    this->erase(key);
                }
            }
            apm_logger::perf("st-dist-kv-get", {{"namespace", this->ns}, {"success", success ? "1" : "0"}},
                             time::now() - begin, 10U);
            return result;
        }
        bool disk_kv::not_expired(const proto::value &val) {
            return val.expire() == 0 || val.expire() > utils::time::now() / 1000;
        }
        void disk_kv::put(const std::string &key, const std::string &value) { this->put(key, value, 0); }

        void disk_kv::put(const std::string &key, const std::string &value, uint32_t expire) {
            auto begin = time::now();
            proto::value val;
            val.set_data(value.c_str());
            if (expire != 0) {
                val.set_expire(utils::time::now() / 1000 + expire);
            } else {
                val.set_expire(expire);
            }
            bool success = db->Put(leveldb::WriteOptions(), key, val.SerializeAsString()).ok();
            apm_logger::perf("st-dist-kv-put", {{"namespace", this->ns}, {"success", success ? "1" : "0"}},
                             time::now() - begin, 10);
        }
        void disk_kv::erase(const std::string &key) { db->Delete(leveldb::WriteOptions(), key); }
        void disk_kv::clear() {
            leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                erase(it->key().ToString());
            }
            delete it;
        }
        disk_kv::disk_kv(const std::string &ns, uint32_t max_size) : abstract_kv(ns, max_size) {
            st::utils::file::mkdirs(KV_FOLDER);
            options.create_if_missing = true;
            options.block_cache = leveldb::NewLRUCache(max_size);
            leveldb::Status status = leveldb::DB::Open(options, KV_FOLDER + ns, &db);
            assert(status.ok());
        }
        void disk_kv::list(std::function<void(const std::string &, const std::string &)> consumer) {
            leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                proto::value val;
                val.ParseFromString(it->value().ToString());
                if (not_expired(val)) {
                    consumer(it->key().ToString(), val.data());
                }
            }
            delete it;
        }
    }// namespace kv
}// namespace st