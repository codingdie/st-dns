//
// Created by codingdie on 2020/5/20.
//

#include <iostream>
#include "DNSClient.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/regex.hpp>
#include "DNSRequest.h"

using namespace std;
using namespace boost::algorithm;

int main(int argc, char *argv[]) {

    char a = 74;
    std::cout << a;
    std::cout << DNSClient::udpDns("baidu.com", "114.114.114.114") << std::endl;
}