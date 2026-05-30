//
// Created by codingdie on 10/12/22.
//

#include "disk_kv.h"
#include "kv.pb.h"
#include "utils/base64.h"
#include "utils/logger.h"
#include "utils/file.h"
#include <leveldb/cache.h>
#include <cstdlib>

static const char *const KV_FOLDER = "/var/lib/st/kv/";
static const char *const KV_FOLDER_TMP = "/tmp/st/kv/";

using namespace st::utils;
namespace st {
    namespace kv {
        static string normalize_kv_folder(const char *folder) {
            string path = folder;
            if (!path.empty() && path.back() != '/') {
                path.push_back('/');
            }
            return path;
        }

        static vector<string> kv_folders() {
            vector<string> folders;
            const char *env_folder = std::getenv("ST_KV_FOLDER");
            if (env_folder != nullptr && env_folder[0] != '\0') {
                folders.emplace_back(normalize_kv_folder(env_folder));
            }
            folders.emplace_back(KV_FOLDER);
            folders.emplace_back(KV_FOLDER_TMP);
            return folders;
        }

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
            std::vector<std::string> keys;
            leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                keys.push_back(it->key().ToString());
            }
            delete it;

            for (const auto &key : keys) {
                erase(key);
            }
        }
        disk_kv::disk_kv(const std::string &ns, uint32_t max_size) : abstract_kv(ns, max_size) {
            options.create_if_missing = true;
            options.block_cache = leveldb::NewLRUCache(max_size);
            leveldb::Status status;
            for (const auto &folder : kv_folders()) {
                if (!st::utils::file::mkdirs(folder)) {
                    continue;
                }
                status = leveldb::DB::Open(options, folder + ns, &db);
                if (status.ok()) {
                    break;
                }
            }
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
