#include "SHM.h"
using namespace st::dns;
using namespace st::utils;

SHM &SHM::read() {
    static SHM read;
    return read;
}
SHM &SHM::write() {
    static SHM write(false);
    return write;
}
SHM::SHM(bool readOnly) : readOnly(readOnly), initSHMSuccess(false) {
    try {
        if (!readOnly) {
            boost::interprocess::shared_memory_object::remove(NAME.c_str());
            segment = new boost::interprocess::managed_shared_memory(boost::interprocess::create_only, NAME.c_str(),
                                                                     1024 * 1024 * 4);
            alloc_inst = new void_allocator(segment->get_segment_manager());
            dnsMap = segment->construct<shm_map>(REVERSE_NAME.c_str())(std::less<shm_map_key_type>(), *alloc_inst);
            initSHMSuccess = true;
        } else {
            relocate();
        }
    } catch (const std::exception &e) {
        Logger::ERROR << "init SHM error!" << e.what() << END;
    }
}
SHM::~SHM() {
    if (!readOnly) {
        if (segment != nullptr) {
            segment->destroy<shm_map>(REVERSE_NAME.c_str());
        }
        boost::interprocess::shared_memory_object::remove(NAME.c_str());
        if (alloc_inst != nullptr) {
            delete alloc_inst;
        }
    }
    if (segment != nullptr) {
        delete segment;
    }
}


void SHM::relocate() {
    try {
        auto segmentNew = new boost::interprocess::managed_shared_memory(boost::interprocess::open_only, NAME.c_str());
        auto dnsMapNew = segmentNew->find<shm_map>(REVERSE_NAME.c_str()).first;
        auto allocInstNew = new void_allocator(segmentNew->get_segment_manager());
        initSHMSuccess = false;
        if (segment != nullptr) {
            delete segment;
        }
        segment = segmentNew;

        if (alloc_inst != nullptr) {
            delete alloc_inst;
        }
        alloc_inst = allocInstNew;

        dnsMap = dnsMapNew;
        initSHMSuccess = true;
    } catch (const std::exception &e) {
        Logger::ERROR << "relocate error!" << e.what() << END;
        initSHMSuccess = false;
    }
}
std::string SHM::get(const std::string &key) {
    if (!initSHMSuccess.load()) {
        return "";
    }
    shm_map_key_type k(key.c_str(), *alloc_inst);
    auto it = dnsMap->find(k);
    if (it != dnsMap->end()) {
        return it->second.c_str();
    }
    return "";
}
void SHM::put(const std::string &key, const std::string &value) {
    if (!initSHMSuccess || readOnly || key.empty() || value.empty()) {
        return;
    }
    shm_map_value_type shm_val(value.c_str(), *alloc_inst);
    shm_map_key_type shm_key(key.c_str(), *alloc_inst);
    auto it = dnsMap->find(shm_key);
    if (it != dnsMap->end()) {
        it->second = shm_val;
    } else {
        shm_pair_type shm_pair(shm_key, shm_val);
        dnsMap->insert(shm_pair);
    }
}
void SHM::add(const std::string &key, const std::string &value) {
    if (!initSHMSuccess || readOnly || value.empty()) {
        return;
    }
    shm_map_value_type v(value.c_str(), *alloc_inst);
    shm_map_key_type k(key.c_str(), *alloc_inst);
    shm_pair_type pr(k, v);
    dnsMap->insert(pr);
}

std::string SHM::query(uint32_t ip) {
    string val = get(to_string(ip));
    if (val.length() > 0) {
        return val;
    }
    return st::utils::ipv4::ipToStr(ip);
}

void SHM::add(uint32_t ip, const std::string &host) {
    add(to_string(ip), host);
}

void SHM::addOrUpdate(uint32_t ip, const std::string host) {
    put(to_string(ip), host);
}
uint16_t getVPortBegin() {
    uint16_t vPortBegin = 53000;
    const char *env = std::getenv("ST_DNS_VIRTUAL_PORT_BEGIN");
    string vPortBeginStr = "";
    if (env != nullptr) {
        vPortBeginStr = env;
        if (vPortBeginStr.length() > 0) {
            vPortBegin = stoi(vPortBeginStr);
        }
    }
    return vPortBegin;
}


uint16_t SHM::calVirtualPort(const std::string &area) {
    auto areaCode = st::areaip::area2Code(area);
    return getVPortBegin() + areaCode;
}
uint16_t SHM::initVirtualPort(uint32_t ip, uint16_t port, const std::string &area) {
    uint16_t vPort = calVirtualPort(area);
    put(to_string(ip) + ":" + to_string(vPort), to_string(port));
    return vPort;
}

std::pair<std::string, uint16_t> SHM::getRealPort(uint32_t ip, uint16_t vPort) {
    string realPortStr = get(to_string(ip) + ":" + to_string(vPort));
    uint16_t realPort = vPort;
    string area = "";
    if (!realPortStr.empty()) {
        realPort = stoi(realPortStr);
        area = st::areaip::code2Area(vPort - getVPortBegin());
    }
    return make_pair(area, realPort);
}
