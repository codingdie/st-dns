//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_DNSMESSAGE_H
#define ST_DNS_DNSMESSAGE_H


#include "STUtils.h"
#include <atomic>
#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <vector>


using namespace std;

class BasicData {
private:
    bool valid = true;
    bool dataOwner = false;

    static void printBit(uint8_t value) {
        for (uint16_t i = 0; i < 8; i++) {
            if (((1U << 7U >> i) & value) > 0) {
                std::cout << 1;
            } else {
                std::cout << 0;
            }
        }
    }

    static void printHex(uint8_t value) {
        char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e'};
        std::cout << hexChars[((value & 0b11110000U) >> 4)] << hexChars[(value & 0b00001111U)];
    }

public:
    uint32_t len = 0;
    uint8_t *data = nullptr;

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

    BasicData(uint32_t len1) : len(len1) {
        data = new uint8_t[((len - 1) / 1024 + 1) * 1024];
        for (int i = 0; i < len; i++) {
            *(data + i) = 0;
        }
        dataOwner = true;
    }

    BasicData(uint8_t *data, uint32_t len) : len(len), data(data) {
    }

    BasicData(uint8_t *data, uint32_t len, bool dataOwner) : dataOwner(dataOwner), len(len), data(data) {
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
    uint8_t responseCode = 0;
    uint8_t qr = 0;
    uint8_t opcode = 0;

    virtual ~DNSHeader() {}

    DNSHeader(uint8_t *data, uint64_t len) : BasicData(data, len) {
        if (len >= DEFAULT_LEN) {
            this->id |= ((uint32_t) * (data) << 8U);
            this->id |= *(data + 1);
            this->len = DEFAULT_LEN;
            this->qr = (*(data + 2) & (0b10000000));
            this->opcode = ((*(data + 2) & (0b01111000)) >> 3);
            this->responseCode = (*(data + 3) & 0x01F);
            st::utils::read(data + 4, questionCount);
            st::utils::read(data + 6, answerCount);
        } else {
            markInValid();
        }
    }


    explicit DNSHeader(uint64_t len) : BasicData(len) {
    }

    static DNSHeader *generateAnswer(uint16_t id, int hostCount, int answerCount) {
        return generate(id, hostCount, 0, answerCount, true);
    }

    static DNSHeader *generate(uint16_t id, int hostCount, int additionalCount, int answerCount, bool isAnswer) {
        auto *dnsHeader = new DNSHeader(DEFAULT_LEN);
        uint16_t tmpData[DEFAULT_LEN / 2];
        tmpData[0] = id;
        tmpData[1] = 0x0100;
        if (isAnswer) {
            tmpData[1] = 0x8000;
            if (answerCount == 0) {
                tmpData[1] = 0x8003;
            }
            tmpData[1] |= 0x0080;
        }
        tmpData[2] = hostCount;
        tmpData[3] = answerCount;
        tmpData[4] = 0;
        tmpData[5] = additionalCount;

        uint8_t *data = dnsHeader->data;
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

    static DNSHeader *generateQuery(int hostCount) {
        return generate(DnsIdGenerator::id16(), hostCount, 0, 0, false);
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
    virtual ~DNSDomain() {}
    explicit DNSDomain(uint64_t size) : BasicData(size) {
    }


    DNSDomain(uint8_t *data, uint64_t len) : BasicData(data, len) {
        uint64_t actualLen = parseDomain(data, len, 0, len, domain);
        if (actualLen == 0) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }

    DNSDomain(uint8_t *data, uint64_t maxTotal, uint64_t begin) : BasicData(data + begin, maxTotal - begin) {
        uint64_t actualLen = parseDomain(data, maxTotal, begin, maxTotal - begin, domain);
        if (actualLen == 0) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }

    DNSDomain(uint8_t *data, uint64_t len, uint64_t begin, int maxParse) : BasicData(data + begin, len - begin) {
        uint64_t actualLen = parseDomain(data, len, begin, maxParse, domain);
        if (actualLen == 0) {
            this->markInValid();
        } else {
            this->len = actualLen;
        }
    }
    static DNSDomain *generateDomain(const string &host);
    static uint64_t parseDomain(uint8_t *allData, uint64_t max, uint64_t begin, uint64_t maxParse, string &domain);
    static string getFIDomain(const string &domain);
    static string removeFIDomain(const string &domain);
};

class DNSQuery : public BasicData {
public:
    enum Type {
        A = 0x01,    //指定计算机 IP 地址。
        NS = 0x02,   //指定用于命名区域的 DNS 名称服务器。
        MD = 0x03,   //指定邮件接收站（此类型已经过时了，使用MX代替）
        MF = 0x04,   //指定邮件中转站（此类型已经过时了，使用MX代替）
        CNAME = 0x05,//指定用于别名的规范名称。
        SOA = 0x06,  //指定用于 DNS 区域的“起始授权机构”。
        MB = 0x07,   //指定邮箱域名。
        MG = 0x08,   //指定邮件组成员。
        MR = 0x09,   //指定邮件重命名域名。
        NULLV = 0x0A,//指定空的资源记录
        WKS = 0x0B,  //描述已知服务。
        PTR = 0x0C,  //如果查询是 IP 地址，则指定计算机名；否则指定指向其它信息的指针。
        HINFO = 0x0D,//指定计算机 CPU 以及操作系统类型。
        MINFO = 0x0E,//指定邮箱或邮件列表信息。
        MX = 0x0F,   //指定邮件交换器。
        TXT = 0x10,  //指定文本信息。
        AAAA = 0x1c, //IPV6资源记录。
        UINFO = 0x64,//指定用户信息。
        UID = 0x65,  //指定用户标识符。
        GID = 0x66,  //指定组名的组标识符。
        ANY = 0xFF   //指定所有数据类型。
    };
    DNSDomain *domain = nullptr;
    static const uint32_t DEFAULT_FLAGS_SIZE = 4;
    static uint8_t DEFAULT_FLAGS[DEFAULT_FLAGS_SIZE];
    Type queryType = Type::A;
    uint16_t queryTypeValue = Type::A;

    virtual ~DNSQuery();

    explicit DNSQuery(uint64_t len) : BasicData(len) {
    }

    DNSQuery(uint8_t *data, uint64_t len) : BasicData(data, len) {
        auto *dnsDomain = new DNSDomain(data, len);
        if (!dnsDomain->isValid()) {
            markInValid();
        } else {
            this->domain = dnsDomain;
            this->len = dnsDomain->len + DEFAULT_FLAGS_SIZE;
            uint16_t typeValue = 0;
            st::utils::read(data + dnsDomain->len, typeValue);
            this->queryTypeValue = typeValue;
            this->queryType = Type(typeValue);
        }
    }

    static DNSQuery *generateDomain(string &host) {
        DNSDomain *domain = DNSDomain::generateDomain(host);
        uint64_t finalDataLen = domain->len + DEFAULT_FLAGS_SIZE;
        DNSQuery *dnsQuery = new DNSQuery(finalDataLen);
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

    virtual ~DNSQueryZone() {
        for (auto query : querys) {
            delete query;
        }
    }

    explicit DNSQueryZone(uint64_t len) : BasicData(len) {
    }

    DNSQueryZone(uint8_t *data, uint64_t len, uint64_t size) : BasicData(data, len) {
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

    static DNSQueryZone *generate(const vector<string> &hosts) {
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
    unordered_set<uint32_t> ipv4s;

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

    DNSResourceZone(uint8_t *original, uint64_t originalMax, uint64_t begin) : BasicData(original + begin, originalMax - begin) {
        uint32_t size = 0;
        uint8_t *curBegin = original + begin;
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

    static DNSResourceZone *generate(uint32_t ip, uint32_t expire) {
        uint32_t size = 2 + 2 + 2 + 4 + 2 + 4;
        DNSResourceZone *resourceZone = new DNSResourceZone(size);
        resourceZone->len = size;
        uint8_t *data1 = resourceZone->data;
        //domain
        *data1 = 0b11000000;
        data1++;
        *data1 = DNSHeader::DEFAULT_LEN;
        data1++;
        //type
        resourceZone->type = DNSQuery::A;
        data1 = st::utils::write(data1, resourceZone->type);
        //resource
        resourceZone->resource = 1;
        data1 = st::utils::write(data1, resourceZone->resource);
        //live time
        resourceZone->liveTime = expire;
        data1 = st::utils::write(data1, resourceZone->liveTime);
        //data szie
        //todo
        // resourceZone->ipv4s = ips;
        uint16_t dataSize = 4;
        data1 = st::utils::write(data1, dataSize);
        data1 = st::utils::write(data1, ip);
        return resourceZone;
    }
};

#endif//ST_DNS_DNSMESSAGE_H
