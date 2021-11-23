
#ifndef ST_POOL_H
#define ST_POOL_H
#include <boost/pool/singleton_pool.hpp>
#include <utility>
#include <unordered_map>
namespace st {
    namespace mem {

#define POOL_CONFIG boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 8, 0
        struct CHUNCK_64 {};
        typedef boost::singleton_pool<CHUNCK_64, sizeof(uint8_t) * 64, POOL_CONFIG> CHUNCK_64_POOl;
        struct CHUNCK_512 {};
        typedef boost::singleton_pool<CHUNCK_512, sizeof(uint8_t) * 512, POOL_CONFIG> CHUNCK_512_POOl;
        struct CHUNCK_1024 {};
        typedef boost::singleton_pool<CHUNCK_1024, sizeof(uint8_t) * 1024, POOL_CONFIG> CHUNCK_1024_POOl;
        struct CHUNCK_2048 {};
        typedef boost::singleton_pool<CHUNCK_2048, sizeof(uint8_t) * 2048, POOL_CONFIG> CHUNCK_2048_POOL;
#undef POOL_CONFIG
        static std::pair<uint8_t *, uint32_t> pmalloc(uint32_t size) {
            if (size <= 64) {
                return std::make_pair((uint8_t *) CHUNCK_64_POOl::malloc(), 64);
            } else if (size <= 512) {
                return std::make_pair((uint8_t *) CHUNCK_512_POOl::malloc(), 512);
            } else if (size <= 1024) {
                return std::make_pair((uint8_t *) CHUNCK_1024_POOl::malloc(), 1024);
            } else {
                return std::make_pair((uint8_t *) CHUNCK_2048_POOL::malloc(), 2048);
            }
        }
        static void pfree(void *ptr, uint32_t size) {
            if (size <= 64) {
                CHUNCK_64_POOl::free(ptr);
            } else if (size <= 512) {
                CHUNCK_512_POOl::free(ptr);
            } else if (size <= 1024) {
                CHUNCK_1024_POOl::free(ptr);
            } else {
                CHUNCK_2048_POOL::free(ptr);
            }
        }
        static void pfree(std::pair<uint8_t *, uint32_t> data) {
            pfree(data.first, data.second);
        }
        static void pgc() {
            CHUNCK_64_POOl::release_memory();
            CHUNCK_512_POOl::release_memory();
            CHUNCK_1024_POOl::release_memory();
            CHUNCK_2048_POOL::release_memory();
        }
    }// namespace mem
}// namespace st
#endif
