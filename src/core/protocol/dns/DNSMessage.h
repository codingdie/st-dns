//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_DNSMESSAGE_H
#define ST_DNS_DNSMESSAGE_H


#include <iostream>
#include <vector>

typedef unsigned char byte;

using namespace std;
static std::atomic_short DNS_ID(1);


class BasicData {

private:
    bool valid = true;
    bool dataOwner = true;

    static void printBit(byte value) {
        for (unsigned int i = 0; i < 8; i++) {
            if (((1U << 7U >> i) & value) > 0) {
                std::cout << 1;
            } else {
                std::cout << 0;
            }
        }
    }

public:

    template<typename ItemType, typename Num>
    static void copy(ItemType *from, ItemType *to, Num len) {
        for (auto i = 0; i < len; i++) {
            *(to + i) = *(from + i);
        }
    };
    unsigned int len = 0;
    byte *data = nullptr;

    void markInValid() {
        this->valid = false;
    }

    bool isValid() const {
        return valid;
    }

    explicit BasicData(unsigned long len) : len(len) {
        data = new byte[len];
    }

    BasicData(byte *data, unsigned long len) : len(len), data(data) {
        dataOwner = false;
    }

    BasicData() = default;


    void print() const {
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

class DNSHeader : public BasicData {
public:
    string host;
    unsigned short id;
    const static int DEFAULT_LEN = 12;
    byte responseCode;

    static DNSHeader *parseResponse(byte *data, int len) {
        DNSHeader *dnsHeader = new DNSHeader(data, DEFAULT_LEN);
        if (len >= DEFAULT_LEN) {
            copy(data, dnsHeader->data, DEFAULT_LEN);
            dnsHeader->id |= (*data << 4);
            dnsHeader->id |= *(data + 1);
            dnsHeader->responseCode = (*(data + 3) & 0x01);
        } else {
            dnsHeader->markInValid();
        }
        return dnsHeader;
    }


    explicit DNSHeader(unsigned long len) : BasicData(len) {

    }

    DNSHeader(byte *data, unsigned long len) : BasicData(data, len) {

    }

    static DNSHeader *generateQuery() {
        auto *dnsHeader = new DNSHeader(DEFAULT_LEN);
        unsigned short tmpData[DEFAULT_LEN / 2];
        tmpData[0] = DNS_ID++;
        tmpData[1] = 0x0100;
        tmpData[2] = 0x0001;
        tmpData[3] = 0;
        tmpData[4] = 0;
        tmpData[5] = 0;
        unsigned char *data = dnsHeader->data;
        unsigned int maskA = 0xFFU << 8U;
        unsigned int maskB = 0xFF;
        for (auto i = 0; i < DEFAULT_LEN / 2; i++) {
            data[i * 2] = ((tmpData[i] & maskA) >> 8U);
            data[i * 2 + 1] = tmpData[i] & maskB;
        }
        dnsHeader->data = data;
        dnsHeader->len = 6 * 2;
        dnsHeader->id = tmpData[0];
        return dnsHeader;
    }

};


class DNSQueryZone : public BasicData {
public:

    explicit DNSQueryZone(unsigned long len) : BasicData(len) {

    }

    DNSQueryZone(byte *data, unsigned long len) : BasicData(data, len) {

    }

    static DNSQueryZone *parseResponse(byte *data, int len) {
        int actualLen = 0;
        bool valid = false;
        while (actualLen <= len) {
            actualLen++;
            byte frameLen = *(data + actualLen);
            if (frameLen == 0 || frameLen > len - actualLen) {
                if (actualLen >= 2 && frameLen == 0) {
                    valid = true;
                }
                break;
            }
            actualLen += frameLen;
        }
        DNSQueryZone *dnsQueryZone;
        if (valid) {
            dnsQueryZone = new DNSQueryZone(data, actualLen);
        } else {
            dnsQueryZone = new DNSQueryZone(data, 0);
            dnsQueryZone->markInValid();
        }

        return dnsQueryZone;
    }

    static DNSQueryZone *generateQuery(const string &host) {
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
        unsigned long finalDataLen = subLens.size() + total + 5;
        DNSQueryZone *dnsQueryZone = new DNSQueryZone(finalDataLen);

        auto hostCharData = dnsQueryZone->data;

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
        *(hostCharData + i + 1) = 0x00;
        *(hostCharData + i + 2) = 0x01;
        *(hostCharData + i + 3) = 0x00;
        *(hostCharData + i + 4) = 0x01;
        return dnsQueryZone;
    }
};


class DNSResponseZone : public BasicData {
public:

};

#endif //ST_DNS_DNSMESSAGE_H
