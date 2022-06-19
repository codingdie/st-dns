//
// Created by codingdie on 2020/5/28.
//

#include "session.h"

using namespace st::dns;
using namespace st::dns::protocol;
st::dns::session::~session() {
    if (response != nullptr) {
        delete response;
    }
}

st::dns::session::session(uint64_t id) : id(id), logger("st-dns"), request(1024) {
}

uint64_t st::dns::session::get_id() const {
    return id;
}

string st::dns::session::get_host() const {
    return request.get_host();
}
dns_query::Type st::dns::session::get_query_type() const {
    return request.get_query_type();
}

uint16_t st::dns::session::get_query_type_value() const {
    return request.get_query_type_value();
}


uint64_t st::dns::session::get_time() const {
    return time;
}

void st::dns::session::set_time(uint64_t time) {
    st::dns::session::time = time;
}
