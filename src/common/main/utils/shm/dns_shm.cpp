#include "dns_shm.h"
using namespace st::dns;
using namespace st::utils;
using namespace boost::interprocess;
st::dns::shm &st::dns::shm::share() {
    static st::dns::shm in;
    return in;
}

void st::dns::shm::add_reverse_record(uint32_t ip, std::string domain) {
    auto domainStr = reverse->get(to_string(ip));
    auto domains = strutils::split(domainStr, ",");
    if (find(domains.begin(), domains.end(), domain) != domains.end()) {
        domains.emplace_back(domain);
        reverse->put(to_string(ip), strutils::join(domains, ","));
    }
}

vector<std::string> shm::reverse_resolve_all(uint32_t ip) {
    return strutils::split(reverse->get(to_string(ip)), ",");
}


std::string st::dns::shm::reverse_resolve(uint32_t ip) {
    auto domains = reverse_resolve_all(ip);
    if (domains.empty() || domains[0].empty()) {
        return st::utils::ipv4::ip_to_str(ip);
    }
    return domains[0];
}

uint16_t get_vport_begin() {
    uint16_t vport_begin = 53000;
    const char *env = std::getenv("ST_DNS_VIRTUAL_PORT_BEGIN");
    std::string vport_begin_str = "";
    if (env != nullptr) {
        vport_begin_str = env;
        if (vport_begin_str.length() > 0) {
            vport_begin = stoi(vport_begin_str);
        }
    }
    return vport_begin;
}


uint16_t st::dns::shm::cal_virtual_port(const std::string &area) {
    auto area_code = st::areaip::area_to_code(area);
    return get_vport_begin() + area_code;
}

uint16_t st::dns::shm::set_virtual_port(uint32_t ip, uint16_t port, const std::string &area) {
    uint16_t vPort = this->cal_virtual_port(area);
    virtual_port->put(to_string(ip) + ":" + to_string(vPort), to_string(port));
    return vPort;
}

std::pair<std::string, uint16_t> st::dns::shm ::get_real_port(uint32_t ip, uint16_t vPort) {
    std::string real_port_str = virtual_port->get(to_string(ip) + ":" + to_string(vPort));
    uint16_t real_port = vPort;
    std::string area = "";
    if (!real_port_str.empty()) {
        real_port = stoi(real_port_str);
        area = st::areaip::code_to_area(vPort - get_vport_begin());
    }
    return make_pair(area, real_port);
}

uint64_t st::dns::shm::free_size() {
    return reverse->free_size();
}