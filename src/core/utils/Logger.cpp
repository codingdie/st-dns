//
// Created by codingdie on 2020/5/19.
//

#include "Logger.h"
#include <ctime>
#include <iostream>
#include <thread>
#include <utility>

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


void Logger::doLog() {
    std::lock_guard<std::mutex> lg(logMutex);
    string time;
    getTime(time);
    int pos = 0;
    int lastPos = 0;
    ostream &st = *(getStream());

    while ((pos = this->str.find("\n", pos)) != this->str.npos) {
        auto line = this->str.substr(lastPos, (pos - lastPos));
        doLog(time, st, line);
        pos += 1;
        lastPos = pos;
    }

    auto line = this->str.substr(lastPos, (this->str.length() - lastPos));
    doLog(time, st, line);
    this->str.clear();
}

void Logger::doLog(const string &time, ostream &st, const string &line) {
    if (this->level >= LEVEL) {
        st << "[" << this_thread::get_id();
        if (traceId != 0) {
            st << "-" << traceId;
        }
        st << "]" << SPLIT << "[" << tag << "]" << SPLIT << time << SPLIT;
        st << line << endl;
    }
}

ostream *Logger::getStream() {
    ostream *stream = &cout;
    if (tag == "ERROR") {
        stream = &cerr;
    }
    return stream;
}

thread_local Logger Logger::TRACE("TRACE", 0);
thread_local Logger Logger::DEBUG("DEBUG", 1);
thread_local Logger Logger::INFO("INFO", 2);
thread_local Logger Logger::ERROR("ERROR", 3);

uint32_t Logger::LEVEL = 2;

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
    this->str.append(SPLIT);
    return *this;
}

Logger::Logger(string tag, uint32_t level) : tag(tag), level(level) {
}

Logger &Logger::operator<<(const string &string) {
    appendStr(string);
    return *this;
}


void Logger::appendStr(const string &info) {
    this->str.append(info).append(SPLIT);
}

Logger &Logger::operator<<(const unordered_set<string> &strs) {
    for (auto str : strs) {
        appendStr(str);
    }
    return *this;
}


thread_local uint64_t Logger::traceId = 0;
