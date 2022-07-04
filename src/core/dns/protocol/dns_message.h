//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_DNS_MESSAGE_H
#define ST_DNS_DNS_MESSAGE_H


#include "utils/utils.h"
#include <atomic>
#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <vector>


using namespace std;
namespace st {
    namespace dns {
        namespace protocol {


            class basic_data {
            private:
                bool valid = true;
                bool owner = false;
                static void print_bit(uint8_t value) {
                    for (uint16_t i = 0; i < 8; i++) {
                        if (((1U << 7U >> i) & value) > 0) {
                            std::cout << 1;
                        } else {
                            std::cout << 0;
                        }
                    }
                }

                static void print_hex(uint8_t value) {
                    static char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e'};
                    std::cout << hex_chars[((value & 0b11110000U) >> 4)] << hex_chars[(value & 0b00001111U)];
                }


            public:
                uint32_t len = 0;
                uint32_t mlen = 0;
                uint8_t *data = nullptr;

                void mark_invalid() {
                    this->valid = false;
                    this->len = 0;
                }

                void mark_valid() {
                    this->valid = true;
                }


                bool is_valid() const {
                    return valid;
                }
                void alloc(uint32_t size);

                basic_data(uint32_t len);


                basic_data(uint8_t *data, uint32_t len) : len(len), data(data){};

                basic_data(uint8_t *data) : data(data){};
                basic_data() = default;

                virtual void print() const {
                    if (data != nullptr) {
                        for (auto i = 0L; i < len; i++) {
                            if (i != 0 && i % 4 == 0 && i != len) {
                                cout << endl;
                            }
                            print_bit(data[i]);
                            std::cout << " ";
                        }
                        cout << endl;
                    }
                }

                virtual void print_hex() const {
                    if (data != nullptr) {
                        for (auto i = 0L; i < len; i++) {
                            if (i != 0 && i % 8 == 0 && i != len) {
                                cout << endl;
                            }
                            print_hex(data[i]);
                            std::cout << " ";
                        }
                        cout << endl;
                    }
                }

                virtual ~basic_data();
            };

            class dns_id_generator {
            public:
                static dns_id_generator INSTANCE;

                static uint16_t id16();

                dns_id_generator();

                virtual ~dns_id_generator();

            private:
                uint16_t generate_id_16() {
                    return id->fetch_add(1);
                }

                std::atomic_int16_t *id;
            };

            class dns_header : public basic_data {
            public:
                uint16_t id = 0U;
                const static uint32_t DEFAULT_LEN = 12;
                uint16_t answerCount = 0;
                uint16_t questionCount = 0;
                uint8_t responseCode = 0;
                uint8_t qr = 0;
                uint8_t opcode = 0;

                virtual ~dns_header() {}

                static dns_header *parse(uint8_t *data, uint32_t len) {
                    if (len >= DEFAULT_LEN) {
                        dns_header *head = new dns_header(data, DEFAULT_LEN);
                        head->id |= ((uint32_t) * (data) << 8U);
                        head->id |= *(data + 1);
                        head->len = DEFAULT_LEN;
                        head->qr = (*(data + 2) & (0b10000000));
                        head->opcode = ((*(data + 2) & (0b01111000)) >> 3);
                        head->responseCode = (*(data + 3) & 0x01F);
                        st::utils::read(data + 4, head->questionCount);
                        st::utils::read(data + 6, head->answerCount);
                        return head;
                    } else {
                        return nullptr;
                    }
                }


                explicit dns_header(uint64_t len) : basic_data(len) {
                }


                explicit dns_header(uint8_t *data, uint32_t len) : basic_data(data, len) {
                }

                static dns_header *generateAnswer(uint8_t *data, uint16_t id, int hostCount, int answerCount) {
                    return generate(data, id, hostCount, 0, answerCount, true);
                }
                static dns_header *generate(uint8_t *data, uint16_t id, int hostCount, int additionalCount, int answerCount, bool isAnswer) {
                    auto *dnsHeader = new dns_header(data, DEFAULT_LEN);
                    init(dnsHeader, id, hostCount, additionalCount, answerCount, isAnswer);
                    return dnsHeader;
                }


                static void init(dns_header *dnsHeader, uint16_t id, int hostCount, int additionalCount, int answerCount, bool isAnswer) {
                    uint8_t *data = dnsHeader->data;
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

                    uint16_t maskA = 0xFFU << 8U;
                    uint16_t maskB = 0xFF;
                    for (auto i = 0L; i < DEFAULT_LEN / 2; i++) {
                        uint16_t ui = tmpData[i] & maskA;
                        data[i * 2] = (ui >> 8U);
                        data[i * 2 + 1] = tmpData[i] & maskB;
                    }
                    dnsHeader->len = 6 * 2;
                    dnsHeader->id = tmpData[0];
                }

                static dns_header *generateQuery(uint8_t *data, int hostCount) {
                    return generate(data, dns_id_generator::id16(), hostCount, 0, 0, false);
                }

                void updateId(uint16_t id) {
                    this->id = id;
                    *(this->data) = (this->id >> 8U);
                    *(this->data + 1) = this->id & 0xFFU;
                }
            };

            class dns_domain : public basic_data {

            public:
                string domain;
                virtual ~dns_domain() {}
                explicit dns_domain(uint64_t size) : basic_data(size) {
                }


                dns_domain(uint8_t *data, uint64_t len) : basic_data(data, len){};
                void parse() {
                    uint64_t actualLen = parse(data, len, 0, len, domain);
                    if (actualLen == 0) {
                        this->mark_invalid();
                    } else {
                        this->len = actualLen;
                    }
                }

                dns_domain(uint8_t *data, uint64_t dataSize, uint64_t begin) : basic_data(data + begin, dataSize - begin) {
                    uint64_t actualLen = parse(data, dataSize, begin, dataSize - begin, domain);
                    if (actualLen == 0) {
                        this->mark_invalid();
                    } else {
                        this->len = actualLen;
                    }
                }

                dns_domain(uint8_t *data, uint64_t dataSize, uint64_t begin, int maxParse) : basic_data(data + begin, dataSize - begin) {
                    uint64_t actualLen = parse(data, dataSize, begin, maxParse, domain);
                    if (actualLen == 0) {
                        this->mark_invalid();
                    } else {
                        this->len = actualLen;
                    }
                }
                static dns_domain *generate(uint8_t *data, const string &host);
                static uint64_t parse(uint8_t *data, uint64_t dataSize, uint64_t begin, uint64_t maxParse, string &domain);
                static string getFIDomain(const string &domain);
                static string removeFIDomain(const string &domain);
            };

            class dns_query : public basic_data {
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
                dns_domain *domain = nullptr;
                static const uint32_t DEFAULT_FLAGS_SIZE = 4;
                static uint8_t DEFAULT_FLAGS[DEFAULT_FLAGS_SIZE];
                Type queryType = Type::A;
                uint16_t queryTypeValue = Type::A;

                virtual ~dns_query();

                void parse() {
                    auto *dnsDomain = new dns_domain(data, len);
                    dnsDomain->parse();
                    if (!dnsDomain->is_valid()) {
                        mark_invalid();
                    } else {
                        this->domain = dnsDomain;
                        this->len = dnsDomain->len + DEFAULT_FLAGS_SIZE;
                        uint16_t typeValue = 0;
                        st::utils::read(data + dnsDomain->len, typeValue);
                        this->queryTypeValue = typeValue;
                        this->queryType = Type(typeValue);
                    }
                }


                dns_query(uint8_t *data, uint64_t len) : basic_data(data, len) {
                }

                static dns_query *generateDomain(uint8_t *data, string &host) {
                    dns_domain *domain = dns_domain::generate(data, host);
                    uint64_t finalDataLen = domain->len + DEFAULT_FLAGS_SIZE;
                    dns_query *dnsQuery = new dns_query(data, finalDataLen);
                    st::utils::copy(DEFAULT_FLAGS, dnsQuery->data, 0U, domain->len, DEFAULT_FLAGS_SIZE);
                    dnsQuery->domain = domain;
                    return dnsQuery;
                }
            };

            class dns_query_zone : public basic_data {
            public:
                vector<dns_query *> querys;
                vector<string> hosts;

                virtual ~dns_query_zone() {
                    for (auto query : querys) {
                        delete query;
                    }
                }

                explicit dns_query_zone(uint64_t len) : basic_data(len) {
                }


                dns_query_zone(uint8_t *data, uint64_t len) : basic_data(data, len) {
                }

                dns_query_zone(uint8_t *data, uint64_t len, uint64_t size) : basic_data(data, len) {
                    uint32_t resetSize = len;
                    uint32_t realSize = 0;
                    while (resetSize > 0 && this->querys.size() < size) {
                        auto query = new dns_query(data, len);
                        query->parse();
                        if (query->is_valid()) {
                            this->querys.push_back(query);
                        } else {
                            this->mark_invalid();
                            break;
                        }
                        resetSize -= query->len;
                        data += query->len;
                        realSize += query->len;
                    }
                    this->len = realSize;
                }

                static dns_query_zone *generate(uint8_t *data, const vector<string> &hosts) {
                    vector<dns_query *> domains;
                    uint32_t size = 0;
                    for (auto it = hosts.begin(); it < hosts.end(); it++) {
                        string host = *it;
                        dns_query *domain = dns_query::generateDomain(data, host);
                        domains.emplace_back(domain);
                        size += domain->len;
                        data += domain->len;
                    }
                    auto *dnsQueryZone = new dns_query_zone(data, size);
                    dnsQueryZone->querys = domains;
                    uint32_t curLen = 0;
                    for (auto it = domains.begin(); it < domains.end(); it++) {
                        dns_query *domain = *it;
                        st::utils::copy(domain->data, dnsQueryZone->data + curLen, domain->len);
                        curLen += domain->len;
                    }
                    for (auto it = hosts.begin(); it < hosts.end(); it++) {
                        dnsQueryZone->hosts.emplace_back(*it);
                    }
                    return dnsQueryZone;
                }
                dns_query_zone *copy(uint8_t *data) {
                    dns_query_zone *zone = new dns_query_zone(data, this->len);
                    st::utils::copy(this->data, zone->data, this->len);
                    return zone;
                }
            };


            class dns_resource_zone : public basic_data {
            public:
                dns_domain *domain = nullptr;
                dns_domain *cname = nullptr;
                uint16_t type = 0;
                uint16_t resource = 0;
                uint32_t liveTime = 0;
                uint16_t length = 0;
                vector<uint32_t> ipv4s;
                const static uint32_t DEFAULT_SINGLE_IP_LEN = 2 + 2 + 2 + 4 + 2 + 4;
                const static uint32_t MAX_LEN = 512;

                virtual ~dns_resource_zone() {
                    if (domain != nullptr) {
                        delete domain;
                    }
                    if (cname != nullptr) {
                        delete cname;
                    }
                }

                dns_resource_zone(uint8_t *data, uint32_t len) : basic_data(data, len) {
                }

                explicit dns_resource_zone(uint32_t len) : basic_data(len) {
                }

                dns_resource_zone(uint8_t *original, uint64_t originalMax, uint64_t begin) : basic_data(original + begin, originalMax - begin) {
                    uint32_t size = 0;
                    uint8_t *curBegin = original + begin;
                    domain = new dns_domain(original, originalMax, begin);
                    if (!domain->is_valid()) {
                        mark_invalid();
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

                        if (dns_query::Type(type) == dns_query::CNAME) {
                            cname = new dns_domain(original, originalMax, (curBegin - original), length);
                            if (cname->is_valid()) {
                                this->len = size;
                            } else {
                                mark_invalid();
                            }
                        } else {
                            if (length % 4 == 0) {
                                for (auto i = 0; i < length / 4; i++) {
                                    uint32_t ipv4 = 0;
                                    st::utils::read(curBegin + i * 4, ipv4);
                                    ipv4s.emplace_back(ipv4);
                                }
                                this->len = size;
                            } else {
                                mark_invalid();
                            }
                        }
                    }
                }

                static dns_resource_zone *generate(uint8_t *data, uint32_t ip, uint32_t expire) {
                    dns_resource_zone *resourceZone = new dns_resource_zone(data, DEFAULT_SINGLE_IP_LEN);
                    uint8_t *data1 = resourceZone->data;
                    //domain
                    *data1 = 0b11000000;
                    data1++;
                    *data1 = dns_header::DEFAULT_LEN;
                    data1++;
                    //type
                    resourceZone->type = dns_query::A;
                    data1 = st::utils::write(data1, resourceZone->type);
                    //resource
                    resourceZone->resource = 1;
                    data1 = st::utils::write(data1, resourceZone->resource);
                    //live time
                    resourceZone->liveTime = expire;
                    data1 = st::utils::write(data1, resourceZone->liveTime);
                    //data szie
                    //todo
                    resourceZone->ipv4s.emplace_back(ip);
                    uint16_t dataSize = 4;
                    data1 = st::utils::write(data1, dataSize);
                    data1 = st::utils::write(data1, ip);
                    return resourceZone;
                }
            };

        }// namespace protocol
    }    // namespace dns
}// namespace st

#endif//ST_DNS_DNS_MESSAGE_H
