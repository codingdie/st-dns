//
// Created by codingdie on 2020/5/20.
//

#include <iostream>
#include "DNSClient.h"


int main(int argc, char *argv[]) {
    std::cout << DNSClient::dns("baidu.com", "8.8.8.8") << std::endl;
}