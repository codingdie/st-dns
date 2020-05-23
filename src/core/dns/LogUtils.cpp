//
// Created by 徐芃 on 2020/5/19.
//

#include "LogUtils.h"
#include <iostream>
#include <time.h>

using namespace std;
static std::mutex logMutex;

void getTime(std::string &strtime) {

    char buff[20];
    time_t now = time(NULL);
    tm *time = localtime(&now);
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", time);
    strtime = buff;
}

void LogUtils::error(string &info) {
    string time;
    getTime(time);
    std::lock_guard<std::mutex> lg(logMutex);
    cout << "[ERROR] " << time << " " << info << endl;
}

void LogUtils::info(string &info) {
    string time;
    getTime(time);
    std::lock_guard<std::mutex> lg(logMutex);
    cout << "[INFO] " << time << "\t" << info << endl;
}
