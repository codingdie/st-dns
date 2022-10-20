#include "pool.h"

static std::atomic<uint64_t> freeNum(0);
static std::atomic<uint64_t> mallocNum(0);
#define POOL_CONFIG boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 64, 0
struct CHUNK_8 {};
typedef boost::singleton_pool<CHUNK_8, sizeof(uint8_t) * 8, POOL_CONFIG> CHUNK_8_POOl;
struct CHUNK_1024 {};
typedef boost::singleton_pool<CHUNK_1024, sizeof(uint8_t) * 1024, POOL_CONFIG> CHUNK_1024_POOl;
struct CHUNK_2048 {};
typedef boost::singleton_pool<CHUNK_2048, sizeof(uint8_t) * 2048, POOL_CONFIG> CHUNK_2048_POOL;
struct CHUNK_4096 {};
typedef boost::singleton_pool<CHUNK_4096, sizeof(uint8_t) * 4096, POOL_CONFIG> CHUNK_4096_POOL;
#undef POOL_CONFIG
namespace st {
    namespace mem {

        uint64_t free_size() { return freeNum.load(); }
        uint64_t malloc_size() { return mallocNum.load(); }
        uint64_t leak_size() { return mallocNum.load() - freeNum.load(); }
        std::pair<uint8_t *, uint32_t> pmalloc(uint32_t size) {
            if (size <= 8) {
                mallocNum.fetch_add(8);
                return std::make_pair((uint8_t *) CHUNK_8_POOl::malloc(), 8);
            } else if (size <= 1024) {
                mallocNum.fetch_add(1024);
                return std::make_pair((uint8_t *) CHUNK_1024_POOl::malloc(), 1024);
            } else if (size <= 2048) {
                mallocNum.fetch_add(2048);
                return std::make_pair((uint8_t *) CHUNK_2048_POOL::malloc(), 2048);
            } else {
                mallocNum.fetch_add(4096);
                return std::make_pair((uint8_t *) CHUNK_4096_POOL::malloc(), 4096);
            }
        }
        void pfree(uint8_t *ptr, uint32_t size) {
            if (size <= 8) {
                freeNum.fetch_add(8);
                CHUNK_8_POOl::free(ptr);
            } else if (size <= 1024) {
                freeNum.fetch_add(1024);
                CHUNK_1024_POOl::free(ptr);
            } else if (size <= 2048) {
                freeNum.fetch_add(2048);
                CHUNK_2048_POOL::free(ptr);
            } else {
                freeNum.fetch_add(4096);
                CHUNK_4096_POOL::free(ptr);
            }
        }
        void pfree(std::pair<uint8_t *, uint32_t> data) { pfree(data.first, data.second); }
        void pgc() {
            CHUNK_8_POOl::release_memory();
            CHUNK_1024_POOl::release_memory();
            CHUNK_2048_POOL::release_memory();
        }
    }// namespace mem
}// namespace st