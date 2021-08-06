//
// Created by codingdie on 2020/5/19.
//

#include "Logger.h"
#include "Base64Utils.h"
#include "StringUtils.h"
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <ctime>
#include <iostream>
#include <regex>
#include <thread>
#include <utility>
using namespace std;
using namespace st::utils;


string toJson(const boost::property_tree::ptree &properties) {
    std::ostringstream st;
    write_json(st, properties, false);
    std::regex reg("\\\"([0-9]+\\.{0,1}[0-9]*)\\\"");
    return std::regex_replace(st.str(), reg, "$1");
}


boost::property_tree::ptree fromJson(const string &json) {
    boost::property_tree::ptree pt;
    std::stringstream sstream(json);
    boost::property_tree::json_parser::read_json(sstream, pt);
    return pt;
}
static std::mutex logMutex;

thread_local Logger Logger::DEBUG("DEBUG", 0);
thread_local Logger Logger::WARN("WARN", 1);
thread_local Logger Logger::INFO("INFO", 2);
thread_local Logger Logger::ERROR("ERROR", 3);
uint32_t Logger::LEVEL = 2;
string Logger::tag = "default";
UDPLogServer Logger::UDP_LOG_SERVER;


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
    if (this->UDP_LOG_SERVER.isValid()) {
        for (auto &line : strutils::split(finalStr, "\n")) {
            if (!line.empty()) {
                UDPLogger::INSTANCE.log(this->UDP_LOG_SERVER, "[" + tag + "] " + line);
            }
        }
    }
    this->str.clear();
}

void Logger::doLog(const string &time, ostream &st, const string &line) {
    if (this->level >= LEVEL) {
        st << time << SPLIT << "[" << levelName << "]" << SPLIT << "[" << this_thread::get_id() << "]" << SPLIT << "["
           << traceId << "]" << SPLIT << line << endl;
    }
}

ostream *Logger::getSTD() {
    ostream *stream = &cout;
    if (levelName == "ERROR") {
        stream = &cerr;
    }
    return stream;
}

bool UDPLogServer::isValid() { return ip.length() > 0 && port > 0; }


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
UDPLogger::UDPLogger() : ctx() {
    worker = new boost::asio::io_context::work(ctx);
    th = new std::thread([=]() { ctx.run(); });
}

UDPLogger::~UDPLogger() {
    ctx.stop();
    delete worker;
    th->join();
}
void UDPLogger::log(const UDPLogServer &server, const string str) {
    ip::udp::endpoint serverEndpoint(ip::make_address_v4(server.ip), server.port);
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


UDPLogServer APMLogger::UDP_LOG_SERVER;
unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, long>>>> APMLogger::STATISTICS;
std::mutex APMLogger::APM_STATISTICS_MUTEX;
boost::asio::io_context APMLogger::IO_CONTEXT;
boost::asio::io_context::work *APMLogger::IO_CONTEXT_WORK = nullptr;
boost::asio::deadline_timer APMLogger::LOG_TIMMER(APMLogger::IO_CONTEXT);
std::thread *APMLogger::LOG_THREAD;

APMLogger::APMLogger(const string name) { this->addDimension("name", name); }

void APMLogger::start() {
    this->startTime = time::now();
    this->lastStepTime = this->startTime;
}

void APMLogger::end() {
    uint64_t cost = time::now() - this->startTime;
    this->addMetric("cost", cost);
    this->addMetric("count", 1);
    string name = dimensions.get<string>("name");
    string dimensionsId = base64::encode(toJson(dimensions));
    for (auto it = metrics.begin(); it != metrics.end(); it++) {
        string metricName = it->first.data();
        long value = boost::lexical_cast<long>(it->second.data());
        std::lock_guard<std::mutex> lg(APM_STATISTICS_MUTEX);
        accumulateMetric(STATISTICS[name][dimensionsId][metricName], value);
    }
}


void APMLogger::step(const string step) {
    uint64_t cost = time::now() - this->lastStepTime;
    this->lastStepTime = time::now();
    this->addMetric(step + "Cost", cost);
}

void APMLogger::accumulateMetric(unordered_map<string, long> &metric, long value) {
    if (metric.empty()) {
        metric["sum"] = 0;
        metric["min"] = numeric_limits<long>::max();
        metric["max"] = numeric_limits<long>::min();
    }
    metric["sum"] += value;
    metric["min"] = min(value, metric["min"]);
    metric["max"] = max(value, metric["max"]);
}

void APMLogger::perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost) {
    perf(name, std::move(dimensions), cost, 1);
}
void APMLogger::perf(const string & name, unordered_map<string, string> &&dimensions, uint64_t cost, uint64_t count) {
    boost::property_tree::ptree pt;
    for (auto it = dimensions.begin(); it != dimensions.end(); it++) {
        pt.put(it->first, it->second);
    }
    pt.put("name", name);
    string id = base64::encode(toJson(pt));
    std::lock_guard<std::mutex> lg(APM_STATISTICS_MUTEX);
    accumulateMetric(STATISTICS[name][id]["count"], count);
    accumulateMetric(STATISTICS[name][id]["cost"], cost);
}
void APMLogger::enable(const string udpServerIP, uint16_t udpServerPort) {
    UDP_LOG_SERVER.ip = udpServerIP;
    UDP_LOG_SERVER.port = udpServerPort;
    IO_CONTEXT_WORK = new boost::asio::io_context::work(IO_CONTEXT);
    LOG_THREAD = new std::thread([&]() { IO_CONTEXT.run(); });
    scheduleLog();
}
void APMLogger::disable() {
    IO_CONTEXT.stop();
    delete IO_CONTEXT_WORK;
    LOG_THREAD->join();
    delete LOG_THREAD;
}

void APMLogger::scheduleLog() {
    LOG_TIMMER.expires_from_now(boost::posix_time::seconds(60));
    LOG_TIMMER.async_wait([](boost::system::error_code ec) {
        std::lock_guard<std::mutex> lg(APM_STATISTICS_MUTEX);
        for (auto it0 = STATISTICS.begin(); it0 != STATISTICS.end(); it0++) {
            for (auto it1 = it0->second.begin(); it1 != it0->second.end(); it1++) {
                boost::property_tree::ptree finalPT;
                boost::property_tree::ptree dimensions = fromJson(base64::decode(it1->first));
                finalPT.insert(finalPT.end(), dimensions.begin(), dimensions.end());
                auto asd = it1->second;
                long count = it1->second["count"]["sum"];
                if (count <= 0) {
                    continue;
                }
                for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
                    if (it2->first != "count") {
                        it2->second["avg"] = count > 0 ? it2->second["sum"] / count : 0;
                        boost::property_tree::ptree metrics;
                        for (auto it3 = it2->second.begin(); it3 != it2->second.end(); it3++) {
                            metrics.put<long>(it3->first, it3->second);
                        }
                        finalPT.put_child(it2->first, metrics);
                    }
                }
                finalPT.put("count", count);
                if (UDP_LOG_SERVER.isValid()) {
                    UDPLogger::INSTANCE.log(UDP_LOG_SERVER, toJson(finalPT));
                }
            }
        }
        STATISTICS.clear();
        scheduleLog();
    });
}

void Logger::init(boost::property_tree::ptree &tree) {
    auto logConfig = tree.get_child_optional("log");
    if (logConfig.is_initialized()) {
        Logger::LEVEL = logConfig.get().get<int>("level", 1);
        auto rawLogServerConfig = logConfig.get().get_child_optional("raw_log_server");
        auto apmLogServerConfig = logConfig.get().get_child_optional("apm_log_server");
        if (rawLogServerConfig.is_initialized()) {
            Logger::UDP_LOG_SERVER.ip = rawLogServerConfig.get().get<string>("ip", "");
            Logger::UDP_LOG_SERVER.port = rawLogServerConfig.get().get<uint16_t>("port", 0);
            Logger::tag = rawLogServerConfig.get().get<string>("tag", "default");
        }
        if (apmLogServerConfig.is_initialized()) {
            string udpServerIP = apmLogServerConfig.get().get<string>("ip", "");
            uint16_t udpServerPort = apmLogServerConfig.get().get<uint16_t>("port", 0);
            APMLogger::enable(udpServerIP, udpServerPort);
        }
    }
}