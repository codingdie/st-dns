//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_DNSREQUEST_H
#define ST_DNS_DNSREQUEST_H

#include <iostream>
#include <DNSMessage.h>

using namespace std;

class DNSRequest : public BasicData {
public:

    DNSRequest(const std::string &host);


};


#endif //ST_DNS_DNSREQUEST_H
