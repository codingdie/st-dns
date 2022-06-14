#include "SHM.h"
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace st::utils;
using namespace boost::interprocess;
std::string st::SHM::NAME = "ST-SHM";
std::string st::SHM::LOCK_NAME = "ST-SHM-LOCK";
st::SHM &st::SHM::share() {
    static SHM in;
    return in;
}
void st::SHM::checkMutexStatus() {
    named_mutex mt(open_or_create, LOCK_NAME.c_str());
    scoped_lock<named_mutex> testlock(mt, boost::interprocess::defer_lock);
    int i = 0;
    while (++i < 3 && !testlock.owns()) {
        sleep(1);
        testlock.try_lock();
    }
    if (i == 3) {
        named_mutex::remove(LOCK_NAME.c_str());
    }
}
st::SHM::SHM() : initSHMSuccess(false) {
    try {
        checkMutexStatus();
        named_mutex mutex(open_or_create, LOCK_NAME.c_str());
        scoped_lock<named_mutex> lock(mutex);
        segment = new managed_shared_memory(open_or_create, NAME.c_str(), 1024 * 1024 * 4);
        void_allocator alloc_inst(segment->get_segment_manager());
        segment->find_or_construct<shm_map>("KV")(std::less<shm_map_key_type>(), alloc_inst);
        initSHMSuccess = true;
    } catch (const std::exception &e) {
        Logger::ERROR << "init SHM error!" << e.what() << END;
    }
}
st::SHM::~SHM() {
    if (segment != nullptr) {
        delete segment;
    }
}

std::string st::SHM::get(const std::string &key) {
    if (!initSHMSuccess || key.empty()) {
        return "";
    }
    named_mutex mutex(open_or_create, LOCK_NAME.c_str());
    scoped_lock<named_mutex> lock(mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_key_type k(key.c_str(), alloc_inst);
    auto it = kv->find(k);
    if (it != kv->end()) {
        return it->second.c_str();
    }
    return "";
}
void st::SHM::put(const std::string &key, const std::string &value) {
    if (!initSHMSuccess || key.empty() || value.empty()) {
        return;
    }
    named_mutex mutex(open_or_create, LOCK_NAME.c_str());
    scoped_lock<named_mutex> lock(mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_value_type shm_val(value.c_str(), alloc_inst);
    shm_map_key_type shm_key(key.c_str(), alloc_inst);
    auto it = kv->find(shm_key);
    if (it != kv->end()) {
        it->second = shm_val;
    } else {
        shm_pair_type shm_pair(shm_key, shm_val);
        kv->insert(shm_pair);
    }
}
void st::SHM::putIfAbsent(const std::string &key, const std::string &value) {
    if (!initSHMSuccess || key.empty() || value.empty()) {
        return;
    }
    named_mutex mutex(open_or_create, LOCK_NAME.c_str());
    scoped_lock<named_mutex> lock(mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_value_type v(value.c_str(), alloc_inst);
    shm_map_key_type k(key.c_str(), alloc_inst);
    shm_pair_type pr(k, v);
    kv->insert(pr);
}
void st::SHM::clear() {
    scoped_lock<named_mutex> lock(mutex());
    segment->find<shm_map>("KV").first->clear();
}

uint64_t st::SHM::freeSize() {
    return segment->get_segment_manager()->get_free_memory();
}