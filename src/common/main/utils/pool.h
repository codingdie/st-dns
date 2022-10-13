
#ifndef ST_POOL_H
#define ST_POOL_H
#include <boost/pool/singleton_pool.hpp>
#include <utility>
#include <unordered_map>
#include <iostream>
#include <atomic>
namespace st {
    namespace mem {
        std::pair<uint8_t *, uint32_t> pmalloc(uint32_t size);
        void pfree(uint8_t *ptr, uint32_t size);
        void pfree(std::pair<uint8_t *, uint32_t> data);
        void pgc();
        uint64_t free_size();
        uint64_t malloc_size();
        uint64_t leak_size();
    }// namespace mem
}// namespace st
#endif
