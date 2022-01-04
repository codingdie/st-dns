
#ifndef ST_POOL_H
#define ST_POOL_H
#include <boost/pool/singleton_pool.hpp>
#include <utility>
#include <unordered_map>
#include <iostream>
namespace st {
    namespace mem {

#define POOL_CONFIG boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 8, 0
        struct CHUNCK_8 {};
        typedef boost::singleton_pool<CHUNCK_8, sizeof(uint8_t) * 64, POOL_CONFIG> CHUNCK_8_POOl;
        struct CHUNCK_1024 {};
        typedef boost::singleton_pool<CHUNCK_1024, sizeof(uint8_t) * 1024, POOL_CONFIG> CHUNCK_1024_POOl;
        struct CHUNCK_1032 {};
        typedef boost::singleton_pool<CHUNCK_1024, sizeof(uint8_t) * 1032, POOL_CONFIG> CHUNCK_1032_POOl;
        struct CHUNCK_2048 {};
        typedef boost::singleton_pool<CHUNCK_2048, sizeof(uint8_t) * 2048, POOL_CONFIG> CHUNCK_2048_POOL;
#undef POOL_CONFIG
        static std::pair<uint8_t *, uint32_t> pmalloc(uint32_t size) {
            // std::cout << "pmalloc:" << size << std::endl;
            if (size <= 8) {
                return std::make_pair((uint8_t *) CHUNCK_8_POOl::malloc(), 8);
            } else if (size <= 1024) {
                return std::make_pair((uint8_t *) CHUNCK_1024_POOl::malloc(), 1024);
            } else if (size <= 1032) {
                return std::make_pair((uint8_t *) CHUNCK_1032_POOl::malloc(), 1032);
            } else {
                return std::make_pair((uint8_t *) CHUNCK_2048_POOL::malloc(), 2048);
            }
        }
        static void pfree(uint8_t *ptr, uint32_t size) {
            // delete ptr;
            // std::cout << "pfree:" << size << std::endl;
            if (size <= 8) {
                CHUNCK_8_POOl::free(ptr);
            } else if (size <= 1024) {
                CHUNCK_1024_POOl::free(ptr);
            } else if (size <= 1032) {
                CHUNCK_1032_POOl::free(ptr);
            } else {
                CHUNCK_2048_POOL::free(ptr);
            }
        }
        static void pfree(std::pair<uint8_t *, uint32_t> data) {
            pfree(data.first, data.second);
        }
        static void pgc() {
            CHUNCK_8_POOl::release_memory();
            CHUNCK_1032_POOl::release_memory();
            CHUNCK_1024_POOl::release_memory();
            CHUNCK_2048_POOL::release_memory();
        }
    }// namespace mem
}// namespace st
#endif
