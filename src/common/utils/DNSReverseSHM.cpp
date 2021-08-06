#include "DNSReverseSHM.h"
using namespace st::utils::dns;
DNSReverseSHM DNSReverseSHM::READ;
DNSReverseSHM::DNSReverseSHM(bool readOnly) : readOnly(readOnly) {
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
        Logger::ERROR << "init DNSReverseSHM error!" << e.what() << END;
    }
}
DNSReverseSHM::~DNSReverseSHM() {
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
void DNSReverseSHM::relocate() {
    try {
        if (segment != nullptr) {
            delete segment;
        }
        segment = new boost::interprocess::managed_shared_memory(boost::interprocess::open_only, NAME.c_str());
        dnsMap = segment->find<shm_map>(REVERSE_NAME.c_str()).first;
        initSHMSuccess = true;
    } catch (const std::exception &e) {
        Logger::ERROR << "relocate error!" << e.what() << END;
        initSHMSuccess = false;
    }
}
std::string DNSReverseSHM::query(uint32_t ip) {
    if (!initSHMSuccess) {
        return st::utils::ipv4::ipToStr(ip);
    }
    auto it = dnsMap->find(ip);
    if (it != dnsMap->end()) {
        return it->second.c_str();
    }
    return st::utils::ipv4::ipToStr(ip);
}

void DNSReverseSHM::add(uint32_t ip, const std::string host) {
    if (!initSHMSuccess || readOnly || host.empty()) {
        return;
    }
    shm_map_value_type shm_host(host.c_str(), *alloc_inst);
    shm_pair_type value(ip, shm_host);
    dnsMap->insert(value);
}

void DNSReverseSHM::addOrUpdate(uint32_t ip, const std::string host) {
    if (!initSHMSuccess || readOnly || host.empty()) {
        return;
    }
    shm_map_value_type shm_host(host.c_str(), *alloc_inst);
    auto it = dnsMap->find(ip);
    if (it != dnsMap->end()) {
        it->second = shm_host;
    } else {
        shm_pair_type value(ip, shm_host);
        dnsMap->insert(value);
    }
}