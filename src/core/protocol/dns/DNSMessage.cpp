//
// Created by codingdie on 2020/5/23.
//

#include "DNSMessage.h"

byte  DNSQuery::DEFAULT_FLAGS[DEFAULT_FLAGS_SIZE] = {0x00, 0x01, 0x00, 0x01};

DNSQuery::~DNSQuery() {
    if (domain != nullptr) {
        delete domain;
    }
};

DnsIdGenerator::DnsIdGenerator() {
    srand(time::now());
    id = new std::atomic_int16_t(rand() % 0xFFFFFF);
};

DnsIdGenerator::~DnsIdGenerator() {
    delete id;
}

DnsIdGenerator DnsIdGenerator::INSTANCE;

uint16_t DnsIdGenerator::id16() {
    return INSTANCE.generateId16();
};