//
// Created by 徐芃 on 2020/5/19.
//

#ifndef ST_DNS_LOGUTILS_H
#define ST_DNS_LOGUTILS_H

#include <string>
#include <chrono>

using namespace std;


class LogUtils {
public:
    static void error(string &info);

    static void info(string &info);

};


#endif //ST_DNS_LOGUTILS_H
