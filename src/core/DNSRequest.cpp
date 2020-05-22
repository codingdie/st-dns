//
// Created by codingdie on 2020/5/21.
//

#include "DNSRequest.h"
#include <chrono>
#include <vector>


DNSRequest::DNSRequest(const std::string &host) {
    HeaderData headerData;
    headerData.print();
    QueryData queryData(host);
    queryData.print();

    unsigned int len = headerData.len + queryData.len;
    byte *data = new byte[len];
    int j = 0;
    for (int i = 0; i < headerData.len; i++) {
        data[j] = headerData.data[i];
        j++;
    }
    for (int i = 0; i < queryData.len; i++) {
        data[j] = queryData.data[i];
        j++;
    }
    this->data = data;
    this->len = len;
}
