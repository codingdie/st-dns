#include "kv.h"
using namespace boost::interprocess;
using namespace std;
std::string st::shm::kv::NAME = "ST-SHM";
std::string st::shm::kv::LOCK_NAME = "ST-SHM-LOCK";
std::unordered_map<std::string, st::shm::kv *> st::shm::kv::INSTANCES;
std::mutex st::shm::kv::create_mutex;

st::shm::kv *st::shm::kv::create(const std::string &ns, uint32_t maxSize) {
    if (INSTANCES.find(ns) != INSTANCES.end()) {
        return INSTANCES.at(ns);
    }
    std::lock_guard<std::mutex> lg(create_mutex);
    if (INSTANCES.find(ns) != INSTANCES.end()) {
        return INSTANCES.at(ns);
    }
    st::shm::kv *shm = new st::shm::kv(ns, maxSize);
    INSTANCES.emplace(make_pair(ns, shm));
    return shm;
}

st::shm::kv *st::shm::kv::share(const std::string &ns) {
    if (INSTANCES.find(ns) != INSTANCES.end()) {
        return INSTANCES.at(ns);
    }
    return nullptr;
}
void st::shm::kv::check_mutex_status() {
    named_mutex mt(open_or_create, mutex_Name().c_str());
    scoped_lock<named_mutex> testlock(mt, boost::interprocess::defer_lock);
    int i = 0;
    while (++i < 3 && !testlock.owns()) {
        sleep(1);
        testlock.try_lock();
    }
    if (i == 3) {
        named_mutex::remove(mutex_Name().c_str());
    }
}
std::string st::shm::kv::shm_name() {
    return NAME + "-" + ns;
}
std::string st::shm::kv::mutex_Name() {
    return LOCK_NAME + "-" + ns;
}

st::shm::kv::kv(const std::string &ns, uint32_t maxSize) : ns(ns) {
    check_mutex_status();
    named_mutex mutex(open_or_create, mutex_Name().c_str());
    scoped_lock<named_mutex> lock(mutex);
    segment = new managed_shared_memory(open_or_create, shm_name().c_str(), maxSize);
    void_allocator alloc_inst(segment->get_segment_manager());
    segment->find_or_construct<shm_map>("KV")(std::less<shm_map_key_type>(), alloc_inst);
}
st::shm::kv::~kv() {
    if (segment != nullptr) {
        delete segment;
    }
}

std::string st::shm::kv::get(const std::string &key) {
    if (key.empty()) {
        return "";
    }
    named_mutex mutex(open_or_create, mutex_Name().c_str());
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
void st::shm::kv::put(const std::string &key, const std::string &value) {
    if (key.empty() || value.empty()) {
        return;
    }
    named_mutex mutex(open_or_create, mutex_Name().c_str());
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
void st::shm::kv::put_if_absent(const std::string &key, const std::string &value) {
    if (key.empty() || value.empty()) {
        return;
    }
    named_mutex mutex(open_or_create, mutex_Name().c_str());
    scoped_lock<named_mutex> lock(mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_value_type v(value.c_str(), alloc_inst);
    shm_map_key_type k(key.c_str(), alloc_inst);
    shm_pair_type pr(k, v);
    kv->insert(pr);
}
void st::shm::kv::clear() {
    named_mutex mutex(open_or_create, mutex_Name().c_str());
    scoped_lock<named_mutex> lock(mutex);
    segment->find<shm_map>("KV").first->clear();
}
uint64_t st::shm::kv::free_size() {
    return this->segment->get_segment_manager()->get_free_memory();
}
uint64_t st::shm::kv::free_size(const std::string &ns) {
    auto shm = share(ns);
    if (shm != nullptr) {
        return shm->free_size();
    }
    return -1;
}