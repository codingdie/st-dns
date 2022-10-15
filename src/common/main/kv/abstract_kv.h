#ifndef ST_ABSTRACT_KV_H
#define ST_ABSTRACT_KV_H

#include <string>

namespace st {
    namespace kv {
        class abstract_kv {
        public:
            abstract_kv(const std::string &ns, uint32_t max_size) : ns(ns), max_size(max_size) {}

            virtual ~abstract_kv() {}

            virtual std::string get(const std::string &key) { return ""; }

            virtual void put(const std::string &key, const std::string &value) { ; }

            virtual void put(const std::string &key, const std::string &value, uint32_t expire) {}

            virtual void erase(const std::string &key){};

            virtual void clear(){};

            virtual void list(std::function<void(const std::string &key, const std::string &value)> consumer){};

        protected:
            std::string ns;
            uint32_t max_size;
        };// namespace kv

    }// namespace kv
}// namespace st
#endif