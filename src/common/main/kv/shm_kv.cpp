#include "shm_kv.h"

#include <utility>
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost::interprocess;
using namespace std;
std::string st::kv::shm_kv::NAME = "ST-SHM";
std::string st::kv::shm_kv::LOCK_NAME = "ST-SHM-LOCK";
std::unordered_map<std::string, st::kv::shm_kv *> st::kv::shm_kv::INSTANCES;
std::mutex st::kv::shm_kv::create_mutex;
typedef boost::interprocess::managed_shared_memory::segment_manager segment_manager_t;
typedef boost::interprocess::allocator<void, segment_manager_t> void_allocator;
typedef boost::interprocess::allocator<char, segment_manager_t> char_allocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, char_allocator> shm_string;
typedef shm_string shm_map_key_type;
typedef shm_string shm_map_value_type;
typedef std::pair<const shm_map_key_type, shm_map_value_type> shm_pair_type;
typedef boost::interprocess::allocator<shm_pair_type, segment_manager_t> shm_pair_allocator;
typedef boost::interprocess::map<shm_map_key_type, shm_map_value_type, std::less<shm_map_key_type>,
                                 shm_pair_allocator>
        shm_map;

st::kv::shm_kv *st::kv::shm_kv::create(const std::string &ns, uint32_t maxSize) {
    if (INSTANCES.find(ns) != INSTANCES.end()) {
        return INSTANCES.at(ns);
    }
    std::lock_guard<std::mutex> lg(create_mutex);
    if (INSTANCES.find(ns) != INSTANCES.end()) {
        return INSTANCES.at(ns);
    }
    auto *shm = new st::kv::shm_kv(ns, maxSize);
    INSTANCES.emplace(make_pair(ns, shm));
    return shm;
}

st::kv::shm_kv *st::kv::shm_kv::share(const std::string &ns) {
    if (INSTANCES.find(ns) != INSTANCES.end()) {
        return INSTANCES.at(ns);
    }
    return nullptr;
}
void st::kv::shm_kv::check_mutex_status() {
    // if (!interprocess_mutex.timed_lock(boost::posix_time::second_clock::local_time() + boost::posix_time::seconds(3))) {
    //     named_mutex::remove(mutex_name().c_str());
    // } else {
    //     mt.unlock();
    // }
}
std::string st::kv::shm_kv::shm_name() { return NAME + "-" + ns; }


st::kv::shm_kv::shm_kv(const std::string &ns, uint32_t maxSize) : abstract_kv(ns, maxSize),
                                                                  interprocess_mutex(open_or_create, (LOCK_NAME + "-" + ns).c_str()) {
    scoped_lock<named_mutex> lock(interprocess_mutex);
    segment = new managed_shared_memory(open_or_create, shm_name().c_str(), max_size);
    void_allocator alloc_inst(segment->get_segment_manager());
    segment->find_or_construct<shm_map>("KV")(std::less<shm_map_key_type>(), alloc_inst);
}
st::kv::shm_kv::~shm_kv() { delete segment; }

std::string st::kv::shm_kv::get(const std::string &key) {
    if (key.empty() || segment == nullptr) {
        return "";
    }
    scoped_lock<named_mutex> lock(interprocess_mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_key_type k(key.c_str(), alloc_inst);
    auto it = kv->find(k);
    if (it != kv->end()) {
        return it->second.c_str();
    }
    return "";
}
void st::kv::shm_kv::put(const std::string &key, const std::string &value) {
    if (key.empty() || value.empty() || segment == nullptr) {
        return;
    }
    scoped_lock<named_mutex> lock(interprocess_mutex);
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
void st::kv::shm_kv::put_if_absent(const std::string &key, const std::string &value) {
    if (key.empty() || value.empty() || segment == nullptr) {
        return;
    }
    scoped_lock<named_mutex> lock(interprocess_mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_value_type v(value.c_str(), alloc_inst);
    shm_map_key_type k(key.c_str(), alloc_inst);
    shm_pair_type pr(k, v);
    kv->insert(pr);
}
void st::kv::shm_kv::clear() {
    scoped_lock<named_mutex> lock(interprocess_mutex);
    segment->find<shm_map>("KV").first->clear();
}
uint64_t st::kv::shm_kv::free_size() { return this->segment->get_segment_manager()->get_free_memory(); }
void st::kv::shm_kv::erase(const std::string &key) {
    scoped_lock<named_mutex> lock(interprocess_mutex);
    void_allocator alloc_inst(segment->get_segment_manager());
    shm_map *kv = segment->find<shm_map>("KV").first;
    shm_map_key_type k(key.c_str(), alloc_inst);
    kv->erase(k);
}
void st::kv::shm_kv::put(const std::string &key, const std::string &value, uint32_t expire) {
    this->put(key, value);
}
