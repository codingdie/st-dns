#include "DnsSHM.h"
using namespace st::dns;
using namespace st::utils;
using namespace boost::interprocess;
st::dns::SHM &st::dns::SHM::share() {
    static st::dns::SHM in;
    return in;
}

std::string st::dns::SHM ::reverse(uint32_t ip) {
    std::string val = st::SHM::share().get(to_string(ip));
    if (val.length() > 0) {
        return val;
    }
    return st::utils::ipv4::ipToStr(ip);
}

uint16_t getVPortBegin() {
    uint16_t vPortBegin = 53000;
    const char *env = std::getenv("ST_DNS_VIRTUAL_PORT_BEGIN");
    std::string vPortBeginStr = "";
    if (env != nullptr) {
        vPortBeginStr = env;
        if (vPortBeginStr.length() > 0) {
            vPortBegin = stoi(vPortBeginStr);
        }
    }
    return vPortBegin;
}


uint16_t st::dns::SHM::calVirtualPort(const std::string &area) {
    auto areaCode = st::areaip::area2Code(area);
    return getVPortBegin() + areaCode;
}

uint16_t st::dns::SHM::setVirtualPort(uint32_t ip, uint16_t port, const std::string &area) {
    uint16_t vPort = this->calVirtualPort(area);
    st::SHM::share().put(to_string(ip) + ":" + to_string(vPort), to_string(port));
    return vPort;
}

std::pair<std::string, uint16_t> st::dns::SHM ::getRealPort(uint32_t ip, uint16_t vPort) {
    std::string realPortStr = st::SHM::share().get(to_string(ip) + ":" + to_string(vPort));
    uint16_t realPort = vPort;
    std::string area = "";
    if (!realPortStr.empty()) {
        realPort = stoi(realPortStr);
        area = st::areaip::code2Area(vPort - getVPortBegin());
    }
    return make_pair(area, realPort);
}
