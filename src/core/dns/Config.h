//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_CONFIG_H
#define ST_DNS_CONFIG_H

#include <string>

using namespace std;

class Config {
public:
    Config();
    Config(string filename);
    string ip = "127.0.0.1";
    int port = 2020;
};


#endif //ST_DNS_CONFIG_H
