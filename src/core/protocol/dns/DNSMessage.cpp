//
// Created by codingdie on 2020/5/23.
//

#include "DNSMessage.h"
#include <boost/pool/singleton_pool.hpp>

uint8_t DNSQuery::DEFAULT_FLAGS[DEFAULT_FLAGS_SIZE] = {0x00, 0x01, 0x00, 0x01};


BasicData::BasicData(uint32_t size) {
    this->alloc(size);
}
void BasicData::alloc(uint32_t size) {
    auto mem = st::mem::pmalloc(size);
    this->data = mem.first;
    this->mlen = mem.second;
    this->len = size;
    for (int i = 0; i < len; i++) {
        *(this->data + i) = 0;
    }
    this->dataOwner = true;
}

BasicData::~BasicData() {
    if (data != nullptr && dataOwner) {
        st::mem::pfree(data, mlen);
    }
}

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

DNSDomain *DNSDomain::generate(uint8_t *data, const string &host) {
    const char *hostChar = host.data();
    vector<pair<unsigned char, unsigned char>> subLens;
    int total = 0;
    int lastBegin = 0;
    int strLen = strlen(hostChar);
    for (auto i = 0; i <= strLen; i++) {
        if (i != strLen) {
            char ch = hostChar[i];

            if (ch == '.') {
                subLens.emplace_back(lastBegin, i - 1);
                lastBegin = i + 1;
            } else {
                total++;
            }
        } else {
            subLens.emplace_back(lastBegin, i - 1);
        }
    }
    uint64_t finalDataLen = subLens.size() + total + 1;

    auto *dnsDomain = new DNSDomain(data, finalDataLen);
    auto hostCharData = dnsDomain->data;
    int i = 0;
    for (auto &pair : subLens) {
        auto len = pair.second - pair.first + 1;
        *(hostCharData + i) = len;
        i++;
        for (int j = pair.first; j <= pair.second; j++) {
            *(hostCharData + i) = hostChar[j];
            i++;
        }
    }
    *(hostCharData + i) = 0x00;
    dnsDomain->domain = host;
    return dnsDomain;
}
uint64_t parseRe(uint8_t *allData, uint64_t allDataSize, uint64_t begin, uint64_t maxParse, string &domain, int depth) {
    if (depth > 20) {
        Logger::ERROR << "DNSDomain parse to many depth!" << END;
        return 0L;
    }
    uint8_t *data = allData + begin;
    int actualLen = 0;
    while (actualLen < maxParse) {
        uint8_t frameLen = *(data + actualLen);
        if ((frameLen & 0b11000000U) == 0b11000000U) {
            uint16_t pos = 0;
            pos |= ((*(data + actualLen) & 0b00111111U) << 8U);
            pos |= *(data + actualLen + 1);
            actualLen += 2;
            if (pos != begin) {
                string domainRe = "";
                uint64_t consume = parseRe(allData, allDataSize, pos, allDataSize - pos, domainRe, depth + 1);
                if (consume != 0) {
                    if (domain.size() != 0) {
                        domain += ".";
                        domain += domainRe;
                    } else {
                        domain = domainRe;
                        break;
                    }

                } else {
                    return 0;
                }
            } else {
                break;
            }

        } else if ((frameLen & 0b11000000U) == 0b00000000U) {
            actualLen++;
            if (frameLen == 0) {
                break;
            } else {
                if (maxParse - actualLen < frameLen) {
                    return 0;
                }
                actualLen += frameLen;
                char domainStr[frameLen + 1];
                domainStr[frameLen] = '\0';
                st::utils::copy(data, domainStr, actualLen - frameLen, 0U, frameLen);
                if (domain.length() != 0) {
                    domain += ".";
                }
                domain += domainStr;
            }
        } else {
            break;
        }
    }
    return actualLen;
}


uint64_t DNSDomain::parse(uint8_t *allData, uint64_t allDataSize, uint64_t begin, uint64_t maxParse, string &domain) {
    return parseRe(allData, allDataSize, begin, maxParse, domain, 0);
}
string DNSDomain::getFIDomain(const string &domain) {
    uint64_t pos = domain.find_last_of('.');
    if (pos == string::npos) {
        return "";
    }
    string fiDomain = domain.substr(pos + 1);
    transform(fiDomain.begin(), fiDomain.end(), fiDomain.begin(), ::toupper);
    return fiDomain;
}

string DNSDomain::removeFIDomain(const string &domain) {
    uint64_t pos = domain.find_last_of('.');
    if (pos == string::npos) {
        return domain;
    }
    return domain.substr(0, pos);
}
