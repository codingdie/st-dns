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
public:
    unsigned int len = 0;
    byte *data = nullptr;

    void print() {
        if (data != nullptr) {
            for (int i = 0; i < len; i++) {
                printBit(data[i]);
                std::cout << " ";
            }
            std::cout << endl;

        }
    }

    void printBit(byte value) {
        for (int i = 0; i < 8; i++) {
            if (((1 << 7 >> i) & value) > 0) {
                std::cout << 1;
            } else {
                std::cout << 0;
            }
        }

    }

    ~BasicData() {
        if (data != nullptr) {
            delete data;
        }
    }
};

class HeaderData : public BasicData {
private:

public:

    HeaderData() {
        const int len = 6;
        this->len = 6 * 2;
        unsigned short tmpData[len];
        tmpData[0] = DNS_ID++;
        tmpData[1] = 0x0100;
        tmpData[2] = 0x0001;
        tmpData[3] = 0;
        tmpData[4] = 0;
        tmpData[5] = 0;
        this->data = new unsigned char[12];
        unsigned int maskA = 0xFF << 8;
        unsigned int maskB = 0xFF;
        for (auto i = 0; i < len; i++) {
            this->data[i * 2] = ((tmpData[i] & maskA) >> 8u);
            this->data[i * 2 + 1] = tmpData[i] & maskB;
        }
    }

};

class QueryData : public BasicData {
public:
    explicit QueryData(const string &host) {
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
        auto *hostCharData = new unsigned char[finalDataLen];
        this->data = hostCharData;
        this->len = finalDataLen;
        int i = 0;
        for (auto &pair : subLens) {
            auto len = pair.second - pair.first + 1;
            hostCharData[i] = len;
            i++;
            for (int j = pair.first; j <= pair.second; j++) {
                hostCharData[i] = hostChar[j];
                i++;
            }
        }
        hostCharData[i] = 0x00;
        hostCharData[i + 1] = 0x00;
        hostCharData[i + 2] = 0x01;
        hostCharData[i + 3] = 0x00;
        hostCharData[i + 4] = 0x01;
    }
};


#endif //ST_DNS_DNSMESSAGE_H
