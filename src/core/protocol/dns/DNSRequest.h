//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_DNSREQUEST_H
#define ST_DNS_DNSREQUEST_H

#include <iostream>
#include "DNSMessage.h"

using namespace std;

class DNSRequest : public BasicData {
public:
    DNSHeader *dnsHeader = nullptr;
    DNSQueryZone *dnsQueryZone = nullptr;
    DNSRequest(const std::string &host);

    virtual ~DNSRequest();


};


#endif //ST_DNS_DNSREQUEST_H
