//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_DNSMESSAGE_H
#define ST_DNS_DNSMESSAGE_H


#include <iostream>
#include <vector>
#include <set>
#include <cstdlib>
#include <atomic>
#include <cstring>

typedef uint8_t byte;
using namespace std;

namespace st {
    namespace utils {

        template<typename ItemType, typename Num>
        static void copy(ItemType *from, ItemType *to, Num len) {
            for (auto i = 0; i < len; i++) {
                *(to + i) = *(from + i);
            }
        };

        template<typename ItemType, typename ItemTypeB, typename NumA, typename NumB, typename NumC>
        static void copy(ItemType *from, ItemTypeB *to, NumA indexFrom, NumB distFrom, NumC len) {
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

        static string join(vector<string> &lists, const char *delimit) {
            string result;
            for (auto value : lists) {
                if (!result.empty()) {
                    result += delimit;
                }
                result += value;
            }
            return result;
        }

        template<typename IntTypeB>
        static byte *write(byte *data, IntTypeB &result) {
            uint8_t len = sizeof(IntTypeB);
            for (uint8_t i = 0; i < len; i++) {
                auto i1 = (len - i - 1) * 8;
                *(data + i) = (((0xFF << i1) & result) >> i1);
            }
            return data + len;
        }
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

    static void printHex(byte value) {
        char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e'};
        std::cout << hexChars[((value & 0b11110000U) >> 4)] << hexChars[(value & 0b00001111U)];

    }

public:
    uint32_t len = 0;
    byte *data = nullptr;

    void markInValid() {
        this->valid = false;
        this->len = 0;
    }

    void markValid() {
        this->valid = true;
    }


    bool isValid() const {
        return valid;
    }

    explicit BasicData(uint32_t len) : len(len) {
        data = new byte[len];
        dataOwner = true;

    }

    BasicData(byte *data, uint32_t len) : len(len), data(data) {
    }

    BasicData(byte *data, uint32_t len, bool dataOwner) : dataOwner(dataOwner), len(len), data(data) {}

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

    virtual void printHex() const {
        if (data != nullptr) {
            for (int i = 0; i < len; i++) {
                if (i != 0 && i % 8 == 0 && i != len) {
                    cout << endl;
                }
                printHex(data[i]);
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

    void setDataOwner(bool dataOwner) {
        BasicData::dataOwner = dataOwner;
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
    const static uint64_t DEFAULT_LEN = 12;
    uint16_t answerCount = 0;
    uint16_t questionCount = 0;
    byte responseCode = 0;

    DNSHeader(byte *data, uint64_t len) : BasicData(data, len) {
        if (len >= DEFAULT_LEN) {
            this->id |= ((uint32_t) *(data) << 8U);
            this->id |= *(data + 1);
            this->len = DEFAULT_LEN;
            this->responseCode = (*(data + 3) & 0x01F);
            st::utils::read(data + 4, questionCount);
            st::utils::read(data + 6, answerCount);
        } else {
            markInValid();
        }
    }


    explicit DNSHeader(uint64_t len) : BasicData(len) {

    }

    static DNSHeader *generate(uint16_t id, int hostCount, bool isAnswer) {
        auto *dnsHeader = new DNSHeader(DEFAULT_LEN);
        uint16_t tmpData[DEFAULT_LEN / 2];
        tmpData[0] = id;
        tmpData[1] = 0x0100;
        if (isAnswer) {
            tmpData[1] = 0x8000;
        }
        if (isAnswer) {
            tmpData[2] = hostCount;
            tmpData[3] = hostCount;
        } else {
            tmpData[2] = hostCount;
            tmpData[3] = 0;
        }

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

    static DNSHeader *generate(int hostCount) {
        return generate(DnsIdGenerator::id16(), hostCount, false);
    }

    void updateId(uint16_t id) {
        this->id = id;
        *(this->data) = (this->id >> 8U);
        *(this->data + 1) = this->id & 0xFFU;
    }

};

class DNSDomain : public BasicData {

public:
    string domain;

    explicit DNSDomain(uint64_t size) : BasicData(size) {

    }

    static DNSDomain *generateDomain(const string &host) {
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
        uint64_t actualLen = parseDomain(data, len, 0, len, domain);
        if (actualLen == 0) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }

    DNSDomain(byte *data, uint64_t maxTotal, uint64_t begin) : BasicData(data + begin, maxTotal - begin) {
        uint64_t actualLen = parseDomain(data, maxTotal, begin, maxTotal - begin, domain);
        if (actualLen == 0) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }

    DNSDomain(byte *data, uint64_t len, uint64_t begin, int maxParse) : BasicData(data + begin, len - begin) {
        uint64_t actualLen = parseDomain(data, len, begin, maxParse, domain);
        if (actualLen == 0) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }

    uint64_t static parseDomain(byte *allData, uint64_t max, uint64_t begin, uint64_t maxParse, string &domain) {
        byte *data = allData + begin;
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
                    uint64_t consume = parseDomain(allData, max, pos, max - pos, domainRe);
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

            } else {
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
            }
        }
        return actualLen;
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
            st::utils::read(data + dnsDomain->len, typeValue);
            this->queryType = Type(typeValue);
        }
    }

    static DNSQuery *generateDomain(string &host) {
        DNSDomain *domain = DNSDomain::generateDomain(host);
        uint64_t finalDataLen = domain->len + DEFAULT_FLAGS_SIZE;
        auto *dnsQuery = new DNSQuery(finalDataLen);
        auto hostCharData = dnsQuery->data;
        st::utils::copy(domain->data, hostCharData, 0U, 0U, dnsQuery->len);
        st::utils::copy(DEFAULT_FLAGS, hostCharData, 0U, domain->len, DEFAULT_FLAGS_SIZE);
        dnsQuery->domain = domain;
        return dnsQuery;
    }

};

class DNSQueryZone : public BasicData {
public:
    vector<DNSQuery *> querys;
    vector<string> hosts;

    ~DNSQueryZone() {
        for (auto query : querys) {
            delete query;
        }
    }

    explicit DNSQueryZone(uint64_t len) : BasicData(len) {

    }

    DNSQueryZone(byte *data, uint64_t len, uint64_t size) : BasicData(data, len) {
        uint32_t resetSize = len;
        uint32_t realSize = 0;
        while (resetSize > 0 && this->querys.size() < size) {
            auto query = new DNSQuery(data, len);
            if (query->isValid()) {
                this->querys.push_back(query);
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

    static DNSQueryZone *generate(vector<string> &hosts) {
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
            st::utils::copy(domain->data, dnsQueryZone->data + curLen, domain->len);
            curLen += domain->len;
        }
        for (auto it = hosts.begin(); it < hosts.end(); it++) {
            dnsQueryZone->hosts.emplace_back(*it);
        }
        return dnsQueryZone;
    }

    static DNSQueryZone *generate(const string &host) {
        vector<string> hosts;
        hosts.emplace_back(host);
        return generate(hosts);
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
    set<uint32_t> ipv4s;

    virtual ~DNSResourceZone() {
        if (domain != nullptr) {
            delete domain;
        }
        if (cname != nullptr) {
            delete cname;
        }
    }

    explicit DNSResourceZone(uint32_t len) : BasicData(len) {

    }

    DNSResourceZone(byte *original, uint64_t originalMax, uint64_t begin) : BasicData(original + begin,
                                                                                      originalMax - begin) {
        uint32_t size = 0;
        byte *curBegin = original + begin;
        domain = new DNSDomain(original, originalMax, begin);
        if (!domain->isValid()) {
            markInValid();
        } else {
            curBegin += domain->len;
            size += domain->len;
            st::utils::read(curBegin, type);
            curBegin += 2;
            size += 2;
            st::utils::read(curBegin, resource);
            curBegin += 2;
            size += 2;
            st::utils::read(curBegin, liveTime);
            curBegin += 4;
            size += 4;
            st::utils::read(curBegin, length);
            curBegin += 2;
            size += 2;
            size += length;

            if (DNSQuery::Type(type) == DNSQuery::CNAME) {
                cname = new DNSDomain(original, originalMax, (curBegin - original), length);
                if (cname->isValid()) {
                    this->len = size;
                } else {
                    markInValid();
                }
            } else {
                if (length % 4 == 0) {
                    for (auto i = 0; i < length / 4; i++) {
                        uint32_t ipv4 = 0;
                        st::utils::read(curBegin + i * 4, ipv4);
                        ipv4s.emplace(ipv4);
                    }
                    this->len = size;
                } else {
                    markInValid();
                }
            }

        }
    }

    static DNSResourceZone *generate(set<uint32_t> ips) {
        uint32_t size = 2 + 2 + 2 + 4 + 2 + 4 * ips.size();
        DNSResourceZone *resourceZone = new DNSResourceZone(size);
        resourceZone->len = size;
        byte *data1 = resourceZone->data;
        //domain
        *data1 = 0b11000000;
        data1++;
        *data1 = DNSHeader::DEFAULT_LEN;
        data1++;
        //type
        resourceZone->type = DNSQuery::A;;
        data1 = st::utils::write(data1, resourceZone->type);
        //resource
        resourceZone->resource = 0;
        data1 = st::utils::write(data1, resourceZone->resource);
        //live time
        resourceZone->liveTime = 60;
        data1 = st::utils::write(data1, resourceZone->liveTime);
        //data szie
        resourceZone->ipv4s = ips;
        uint16_t dataSize = (uint16_t) (4 * ips.size());;
        data1 = st::utils::write(data1, dataSize);
        for (uint32_t ip:ips) {
            data1 = st::utils::write(data1, ip);
        }
        return resourceZone;
    }

};

#endif //ST_DNS_DNSMESSAGE_H
