//
// Created by 徐芃 on 2020/5/22.
//

#ifndef ST_DNS_DNSRESPONSE_H
#define ST_DNS_DNSRESPONSE_H


#include <iostream>
#include "DNSMessage.h"

using namespace std;

class DNSResponse : public BasicData {
public:
    DNSHeader *header;
    DNSQueryZone *queryZone;
    DNSResponseZone *responseZone;


    DNSResponse(byte *data, unsigned long len);

};


#endif //ST_DNS_DNSRESPONSE_H
