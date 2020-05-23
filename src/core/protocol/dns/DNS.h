#ifndef DNS_H
#define DNS_H


#include "DNSMessage.h"
#include "DnsRequest.h"
#include "DNSResponse.h"

static string ipToStr(uint32_t ip) {
    string ipStr;
    ipStr += to_string((ip & 0xFF000000U) >> 24U);
    ipStr += ".";
    ipStr += to_string((ip & 0x00FF0000U) >> 16U);
    ipStr += ".";
    ipStr += to_string((ip & 0x0000FF00U) >> 8U);
    ipStr += ".";
    ipStr += to_string((ip & 0x000000FFU));
    return ipStr;
}

static string ipsToStr(vector<uint32_t> &ips) {
    string ipStr;
    for (uint32_t ip:ips) {
        ipStr += ipToStr(ip);
        ipStr += " ";
    }
    return ipStr;
}


#endif