//
// Created by 徐芃 on 2020/5/19.
//

#include "LogUtils.h"
#include <iostream>
#include <time.h>

using namespace std;

void getTime(std::string &strtime) {
    char buff[20];
    time_t now = time(NULL);
    tm *time = localtime(&now);
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", time);
    strtime = buff;
}

void LogUtils::error(string info) {
    cout << "[ERROR] " << info << endl;
}
