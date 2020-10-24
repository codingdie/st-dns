//
// Created by codingdie on 2020/5/28.
//

#include "DNSSession.h"

DNSSession::~DNSSession() {

}

DNSSession::DNSSession(uint64_t id) : id(id) {
}

uint64_t DNSSession::getId() const {
    return id;
}



