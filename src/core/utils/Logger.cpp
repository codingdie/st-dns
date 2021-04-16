//
// Created by codingdie on 2020/5/19.
//

#include "Logger.h"
#include "StringUtils.h"
#include <boost/property_tree/json_parser.hpp>
#include <ctime>
#include <iostream>
#include <regex>
#include <thread>
#include <utility>
using namespace std;
using namespace st::utils;
static std::mutex logMutex;

thread_local Logger Logger::TRACE("TRACE", 0);
thread_local Logger Logger::DEBUG("DEBUG", 1);
thread_local Logger Logger::INFO("INFO", 2);
thread_local Logger Logger::ERROR("ERROR", 3);
uint32_t Logger::LEVEL = 2;
string Logger::udpServerIP = "";
uint16_t Logger::udpServerPort = 0;
string Logger::tag = "default";


void Logger::doLog() {
    std::lock_guard<std::mutex> lg(logMutex);
    string time = time::nowStr();
    int pos = 0;
    int lastPos = 0;
    std::ostringstream st;
    while ((pos = this->str.find("\n", pos)) != this->str.npos) {
        auto line = this->str.substr(lastPos, (pos - lastPos));
        doLog(time, st, line);
        pos += 1;
        lastPos = pos;
    }
    auto line = this->str.substr(lastPos, (this->str.length() - lastPos));
    doLog(time, st, line);
    string finalStr = st.str();
    STDLogger::INSTANCE.log(finalStr, getSTD());
    if (this->enableUDPLogger()) {
        for (auto &line : strutils::split(finalStr, "\n")) {
            if (!line.empty()) {
                UDPLogger::INSTANCE.log(this->udpServerIP, this->udpServerPort, "[" + tag + "] " + line);
            }
        }
    }
    this->str.clear();
}

void Logger::doLog(const string &time, ostream &st, const string &line) {
    if (this->level >= LEVEL) {
        st << time << SPLIT << "[" << levelName << "]" << SPLIT << "[" << this_thread::get_id() << "]" << SPLIT << "[" << traceId << "]" << SPLIT
           << line << endl;
    }
}

ostream *Logger::getSTD() {
    ostream *stream = &cout;
    if (levelName == "ERROR") {
        stream = &cerr;
    }
    return stream;
}
bool Logger::enableUDPLogger() { return udpServerIP.length() > 0 && udpServerPort > 0; }


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

Logger::Logger(string levelName, uint32_t level) : levelName(levelName), level(level) {}

Logger &Logger::operator<<(const string &string) {
    appendStr(string);
    return *this;
}


void Logger::appendStr(const string &info) { this->str.append(info).append(SPLIT); }

Logger &Logger::operator<<(const unordered_set<string> &strs) {
    for (auto str : strs) {
        appendStr(str);
    }
    return *this;
}

thread_local uint64_t Logger::traceId = 0;
UDPLogger UDPLogger::INSTANCE;
UDPLogger::UDPLogger() : ctx(), worker(ctx) {
    thread th([=]() { ctx.run(); });
    th.detach();
}
void UDPLogger::log(const string ip, const int port, const string str) {
    ip::udp::endpoint serverEndpoint(ip::make_address_v4(ip), port);
    boost::system::error_code error;
    ip::udp::socket sc(ctx, ip::udp::endpoint(ip::udp::v4(), 0));
    sc.async_send_to(buffer(str), serverEndpoint, [=](boost::system::error_code error, std::size_t size) {
        if (error) {
            cerr << error.message() << endl;
        }
    });
}
STDLogger STDLogger::INSTANCE;
STDLogger::STDLogger() {}
void STDLogger::log(const string str, ostream *st) { *st << str; }

APMLogger::APMLogger(const string name, const string traceId) {
    props.put<string>("name", name);
    props.put<string>("traceId", traceId);
}

void APMLogger::start() {
    this->startTime = time::now();
    this->lastStepTime = this->startTime;
}

void APMLogger::end() {
    boost::property_tree::ptree properties;
    uint64_t cost = time::now() - this->startTime;
    properties.put<string>("step", "all");
    properties.put<uint64_t>("cost", cost);
    this->log(properties);
}


void APMLogger::step(const string step, const boost::property_tree::ptree &properties) {
    uint64_t cost = time::now() - this->lastStepTime;
    this->lastStepTime = time::now();
    boost::property_tree::ptree pt;
    pt.put<string>("step", step);
    pt.put<uint64_t>("cost", cost);
    if (!properties.empty()) {
        pt.add_child("extra", properties);
    }
    this->log(pt);
}

void APMLogger::step(const string step) {
    boost::property_tree::ptree properties;
    this->step(step, properties);
}
void APMLogger::log(boost::property_tree::ptree &properties) {
    properties.insert(properties.end(), props.begin(), props.end());
    doLog(properties);
}

void APMLogger::doLog(boost::property_tree::ptree &properties) {
    std::ostringstream st;
    write_json(st, properties, false);
    std::regex reg("\\\"([0-9]+\\.{0,1}[0-9]*)\\\"");
    string str = std::regex_replace(st.str(), reg, "$1");
    if (udpServerIP.size() > 0 && udpServerPort > 0) {
        UDPLogger::INSTANCE.log(udpServerIP, udpServerPort, str);
    }
}
void APMLogger::perf(const string id, const uint32_t cost, boost::property_tree::ptree &properties) {
    boost::property_tree::ptree pt;
    pt.put<string>("name", id);
    pt.put<uint64_t>("cost", cost);
    if (!properties.empty()) {
        pt.add_child("extra", properties);
    }
    doLog(pt);
}


string APMLogger::udpServerIP = "";
uint16_t APMLogger::udpServerPort = 0;


void Logger::init(boost::property_tree::ptree &tree) {
    auto logConfig = tree.get_child_optional("log");
    if (logConfig.is_initialized()) {
        Logger::LEVEL = logConfig.get().get<int>("level", 1);
        auto rawLogServerConfig = logConfig.get().get_child_optional("raw_log_server");
        auto apmLogServerConfig = logConfig.get().get_child_optional("apm_log_server");
        if (rawLogServerConfig.is_initialized()) {
            Logger::udpServerIP = rawLogServerConfig.get().get<string>("ip", "");
            Logger::udpServerPort = rawLogServerConfig.get().get<uint16_t>("port", 0);
            Logger::tag = rawLogServerConfig.get().get<string>("tag", "default");
        }
        if (apmLogServerConfig.is_initialized()) {
            APMLogger::udpServerIP = apmLogServerConfig.get().get<string>("ip", "");
            APMLogger::udpServerPort = apmLogServerConfig.get().get<uint16_t>("port", 0);
        }
    }
}