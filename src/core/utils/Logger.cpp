//
// Created by codingdie on 2020/5/19.
//

#include "Logger.h"
#include <iostream>
#include <ctime>
#include <utility>
#include <thread>

using namespace std;
using namespace st::utils;
static std::mutex logMutex;

void Logger::getTime(std::string &timeStr) {
    char buff[20];
    time_t now = time(nullptr);
    tm *time = localtime(&now);
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", time);
    timeStr = buff;
}


void Logger::doLog(const string &level, const string &info) {
    string time;
    getTime(time);
    string result = " [" + level + "] " + time + " " + info;
    std::lock_guard<std::mutex> lg(logMutex);
    this->str.clear();
//    if (this->tag == "DEBUG") {
//        return;
//    }
    if (this->tag == "ERROR") {
        cerr << "[" << this_thread::get_id() << "]" << result << endl;
    } else {
        cout << "[" << this_thread::get_id() << "]" << result << endl;
    }
}

thread_local Logger Logger::INFO("INFO");
thread_local Logger Logger::ERROR("ERROR");
thread_local Logger Logger::DEBUG("DEBUG");


Logger &Logger::operator<<(const char *log) {
    appendStr(log);
    return *this;
}

Logger &Logger::operator<<(char *log) {
    appendStr(log);
    return *this;
}

Logger &Logger::operator<<(char ch) {
    this->str.push_back(ch);
    this->str.append(" ");
    return *this;
}

Logger::Logger(string tag) : tag(tag) {
}

Logger &Logger::operator<<(const string &string) {
    appendStr(string);
    return *this;
}


void Logger::appendStr(const string &info) {
    this->str.append(info).append(" ");
}

Logger &Logger::operator<<(const set<string> &strs) {
    for (auto str:strs) {
        appendStr(str);
    }
    return *this;
}

