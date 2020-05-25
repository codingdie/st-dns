//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_DNSMESSAGE_H
#define ST_DNS_DNSMESSAGE_H


#include <iostream>
#include <vector>
#include <cstdlib>

typedef uint8_t byte;
using namespace std;

template<typename ItemType, typename Num>
static void copy(ItemType *from, ItemType *to, Num len) {
    for (auto i = 0; i < len; i++) {
        *(to + i) = *(from + i);
    }
};


template<typename ItemType, typename ItemTypeB, typename Num>
static void copy(ItemType *from, ItemTypeB *to, Num indexFrom, Num distFrom, Num len) {
    for (auto i = 0; i < len; i++) {
        *(to + distFrom + i) = *(from + indexFrom + i);
    }
};


template<typename Num>
static void toBytes(byte *byteArr, Num num) {
    uint8_t len = sizeof(Num);
    for (auto i = 0; i < len; i++) {
        uint64_t move = (len - i - 1) * 8U;
        uint64_t mask = 0xFFU << move;
        *(byteArr + i) = (num & mask) >> move;
    }
};

template<typename IntTypeB>
static void read(const byte *data, IntTypeB &result) {
    uint8_t len = sizeof(IntTypeB);
    for (uint8_t i = 0; i < len; i++) {
        byte val = *(data + i);
        uint32_t bitMove = (len - i - 1) * 8U;
        uint32_t valFinal = val << bitMove;
        result |= valFinal;
    }
}

class BasicData {
private:
    bool valid = true;
    bool dataOwner = false;

    static void printBit(byte value) {
        for (uint16_t i = 0; i < 8; i++) {
            if (((1U << 7U >> i) & value) > 0) {
                std::cout << 1;
            } else {
                std::cout << 0;
            }
        }
    }

public:
    uint32_t len = 0;
    byte *data = nullptr;

    void markInValid() {
        this->valid = false;
        this->len = 0;
    }

    bool isValid() const {
        return valid;
    }

    explicit BasicData(uint64_t len) : len(len) {
        data = new byte[len];
        dataOwner = true;

    }

    BasicData(byte *data, uint64_t len) : len(len), data(data) {
    }

    BasicData() = default;


    virtual void print() const {
        if (data != nullptr) {
            for (int i = 0; i < len; i++) {
                if (i != 0 && i % 4 == 0 && i != len) {
                    cout << endl;
                }
                printBit(data[i]);
                std::cout << " ";

            }
            cout << endl;

        }
    }

    ~BasicData() {
        if (data != nullptr && dataOwner) {
            delete data;
        }
    }


};

class DnsIdGenerator {
public:
    static DnsIdGenerator INSTANCE;

    static uint16_t id16();

    DnsIdGenerator();

    virtual ~DnsIdGenerator();

private:

    uint16_t generateId16() {
        return id->fetch_add(1);
    }

    std::atomic_int16_t *id;

};

class DNSHeader : public BasicData {
public:
    uint16_t id = 0U;
    const static int DEFAULT_LEN = 12;
    uint16_t answerCount = 0;
    uint16_t questionCount = 0;
    byte responseCode = 0;

    DNSHeader(byte *data, uint64_t len) : BasicData(data, len) {
        if (len >= DEFAULT_LEN) {
            this->id |= ((uint32_t) *(data) << 8U);
            this->id |= *(data + 1);
            this->len = DEFAULT_LEN;
            this->responseCode = (*(data + 3) & 0x01U);
            read(data + 4, questionCount);
            read(data + 6, answerCount);
        } else {
            markInValid();
        }
    }


    explicit DNSHeader(uint64_t len) : BasicData(len) {

    }

    static DNSHeader *generateQuery(int hostCount) {
        auto *dnsHeader = new DNSHeader(DEFAULT_LEN);
        uint16_t tmpData[DEFAULT_LEN / 2];
        tmpData[0] = DnsIdGenerator::id16();
        tmpData[1] = 0x0100;
        tmpData[2] = hostCount;
        tmpData[3] = 0;
        tmpData[4] = 0;
        tmpData[5] = 0;

        byte *data = dnsHeader->data;
        uint16_t maskA = 0xFFU << 8U;
        uint16_t maskB = 0xFF;
        for (auto i = 0; i < DEFAULT_LEN / 2; i++) {
            uint16_t ui = tmpData[i] & maskA;
            data[i * 2] = (ui >> 8U);
            data[i * 2 + 1] = tmpData[i] & maskB;
        }
        dnsHeader->data = data;
        dnsHeader->len = 6 * 2;
        dnsHeader->id = tmpData[0];
        return dnsHeader;
    }


};

class DNSDomain : public BasicData {

public:
    string domain;

    explicit DNSDomain(uint64_t size) : BasicData(size) {

    }

    static DNSDomain *generateDomain(string &host) {
        const char *hostChar = host.data();
        vector<pair<unsigned char, unsigned char  >> subLens;
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

        auto *dnsDomain = new DNSDomain(finalDataLen);
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

    DNSDomain(byte *data, uint64_t len) : BasicData(data, len) {
        int actualLen = 0;
        bool valid = false;
        while (actualLen <= len) {
            uint32_t frameLen = *(data + actualLen);
            actualLen++;

            if (frameLen == 0 || frameLen > len - actualLen) {
                if (actualLen >= 2 && frameLen == 0) {
                    valid = true;
                }
                break;
            }
            actualLen += frameLen;
            char domainStr[frameLen + 1];
            domainStr[frameLen] = '\0';
            copy(data, domainStr, actualLen - frameLen, 0U, frameLen);
            if (domain.length() != 0) {
                domain += ".";
            }
            domain += (domainStr);
        }

        if (!valid) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }
};

class DNSQuery : public BasicData {
public:
    enum Type {
        A = 1, CNAME = 5
    };
    DNSDomain *domain = nullptr;
    static const uint32_t DEFAULT_FLAGS_SIZE = 4;
    static byte DEFAULT_FLAGS[DEFAULT_FLAGS_SIZE];
    Type queryType = Type::A;

    virtual ~DNSQuery();

    explicit DNSQuery(uint64_t len) : BasicData(len) {

    }

    DNSQuery(byte *data, uint64_t len) : BasicData(data, len) {
        auto *dnsDomain = new DNSDomain(data, len);
        if (!dnsDomain->isValid()) {
            markInValid();
        } else {
            this->domain = dnsDomain;
            this->len = dnsDomain->len + DEFAULT_FLAGS_SIZE;
            uint16_t typeValue = 0;
            read(data + dnsDomain->len, typeValue);
            this->queryType = Type(typeValue);
        }
    }

    static DNSQuery *generateDomain(string &host) {
        DNSDomain *domain = DNSDomain::generateDomain(host);
        uint64_t finalDataLen = domain->len + DEFAULT_FLAGS_SIZE;
        auto *dnsQuery = new DNSQuery(finalDataLen);
        auto hostCharData = dnsQuery->data;
        copy(domain->data, hostCharData, 0U, 0U, dnsQuery->len);
        copy(DEFAULT_FLAGS, hostCharData, 0U, domain->len, DEFAULT_FLAGS_SIZE);
        dnsQuery->domain = domain;
        return dnsQuery;
    }

};

class DNSQueryZone : public BasicData {
public:
    vector<DNSQuery *> querys;

    ~DNSQueryZone() {
        for (auto query : querys) {
            delete query;
        }
    }

    explicit DNSQueryZone(uint64_t len) : BasicData(len) {

    }

    DNSQueryZone(byte *data, uint64_t len, int size) : BasicData(data, len) {
        uint32_t resetSize = len;
        uint32_t realSize = 0;
        while (resetSize > 0 && this->querys.size() < size) {
            auto query = new DNSQuery(data, len);
            if (query->isValid()) {
                this->querys.emplace_back(query);
            } else {
                this->markInValid();
                break;
            }
            resetSize -= query->len;
            data += query->len;
            realSize += query->len;
        }
        this->len = realSize;

    }

    static DNSQueryZone *generateQuery(vector<string> &hosts) {
        vector<DNSQuery *> domains;
        uint32_t size = 0;
        for (auto it = hosts.begin(); it < hosts.end(); it++) {
            string host = *it;
            DNSQuery *domain = DNSQuery::generateDomain(host);
            domains.emplace_back(domain);
            size += domain->len;
        }
        auto *dnsQueryZone = new DNSQueryZone(size);
        dnsQueryZone->querys = domains;
        uint32_t curLen = 0;
        for (auto it = domains.begin(); it < domains.end(); it++) {
            DNSQuery *domain = *it;
            copy(domain->data, dnsQueryZone->data + curLen, domain->len);
            curLen += domain->len;
        }
        return dnsQueryZone;
    }
};


class DNSResourceZone : public BasicData {
public:
    DNSDomain *domain = nullptr;
    DNSDomain *cname = nullptr;
    uint16_t type = 0;
    uint16_t resource = 0;
    uint32_t liveTime = 0;
    uint16_t length = 0;
    uint32_t ipv4 = 0;

    virtual ~DNSResourceZone() {
        delete domain;
        delete cname;

    }

    DNSResourceZone(byte *original, byte *curBegin, uint64_t originalMax, uint64_t curMax) : BasicData(curBegin,
                                                                                                       curMax) {
        uint32_t size = 0;
        if ((*curBegin & 0b11000000U) == 0b11000000U) {
            uint16_t pos = 0;
            pos |= ((*curBegin & 0b00111111U) << 8U);
            pos |= *(curBegin + 1);
            size += 2;
            curBegin += 2;
            domain = new DNSDomain((original + pos), originalMax - pos);

        } else {
            domain = new DNSDomain(curBegin, curMax);
            if (domain->isValid()) {
                size += domain->len;
                curBegin += domain->len;
            }

        }

        if (!domain->isValid()) {
            markInValid();
        } else {
            read(curBegin, type);
            curBegin += 2;
            size += 2;
            read(curBegin, resource);
            curBegin += 2;
            size += 2;
            read(curBegin, liveTime);
            curBegin += 4;
            size += 4;
            read(curBegin, length);
            curBegin += 2;
            size += 2;
            size += length;

            if (DNSQuery::Type(type) == DNSQuery::CNAME) {
                cname = new DNSDomain(curBegin, length);
                if (cname->isValid()) {
                    this->len = size;
                } else {
                    markInValid();
                }
            } else {
                if (length == 4) {
                    read(curBegin, ipv4);
                    this->len = size;
                } else {
                    markInValid();
                }
            }

        }
    }


};

#endif //ST_DNS_DNSMESSAGE_H
