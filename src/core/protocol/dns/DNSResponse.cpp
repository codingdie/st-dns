//
// Created by 徐芃 on 2020/5/22.
//

#include "DNSResponse.h"


DNSResponse::DNSResponse(byte *data, unsigned long len) : BasicData(data, len) {
    BasicData::copy(data, this->data, len);
    this->header = DNSHeader::parseResponse(this->data, len);
    byte *queryZone = (data + this->header->len);
    this->queryZone = DNSQueryZone::parseResponse(queryZone, len);
}
