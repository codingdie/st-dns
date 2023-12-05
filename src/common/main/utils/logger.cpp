//
// Created by codingdie on 2020/5/19.
//

#include "logger.h"
#include "base64.h"
#include "byte.h"
#include "pool.h"
#include "string_utils.h"
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <regex>
#include <thread>
#include <utility>
using namespace std;
using namespace st::utils;


string to_json(const boost::property_tree::ptree &properties) {
    std::ostringstream st;
    write_json(st, properties, false);
    std::regex reg(R"(\"([0-9]+\.{0,1}[0-9]*)\")");
    return std::regex_replace(st.str(), reg, "$1");
}


boost::property_tree::ptree fromJson(const string &json) {
    boost::property_tree::ptree pt;
    std::stringstream sstream(json);
    boost::property_tree::json_parser::read_json(sstream, pt);
    return pt;
}
static std::mutex logMutex;

thread_local logger logger::DEBUG("DEBUG", 0);
thread_local logger logger::WARN("WARN", 1);
thread_local logger logger::INFO("INFO", 2);
thread_local logger logger::ERROR("ERROR", 3);
uint32_t logger::LEVEL = 2;
string logger::tag = "default";
udp_log_server logger::UDP_LOG_SERVER;


void logger::do_log() {
    if (this->level < LEVEL) {
        this->str.clear();
        return;
    }
    std::lock_guard<std::mutex> lg(logMutex);
    string time = time::now_str();
    string::size_type pos = 0L;
    int lastPos = 0;
    std::ostringstream st;
    while ((pos = this->str.find('\n', pos)) != std::string::npos) {
        auto line = this->str.substr(lastPos, (pos - lastPos));
        do_log(time, st, line);
        pos += 1;
        lastPos = pos;
    }
    auto str_line = this->str.substr(lastPos, (this->str.length() - lastPos));
    do_log(time, st, str_line);
    string finalStr = st.str();
    st::utils::std_logger::log(finalStr, get_std());
    if (st::utils::logger::UDP_LOG_SERVER.is_valid()) {
        for (auto &line : strutils::split(finalStr, "\n")) {
            if (!line.empty()) {
                udp_logger::INSTANCE.log(st::utils::logger::UDP_LOG_SERVER, "[" + tag + "] " + line);
            }
        }
    }
    this->str.clear();
}

void logger::do_log(const string &time, ostream &st, const string &line) {
    if (this->level >= LEVEL) {
        st << time << SPLIT << "[" << levelName << "]" << SPLIT << "[" << this_thread::get_id() << "]" << SPLIT << "["
           << traceId << "]" << SPLIT << line << endl;
    }
}

ostream *logger::get_std() {
    ostream *stream = &cout;
    if (levelName == "ERROR") {
        stream = &cerr;
    }
    return stream;
}

bool udp_log_server::is_valid() const { return ip.length() > 0 && port > 0; }


logger &logger::operator<<(const char *log) {
    append_str(log);
    return *this;
}

logger &logger::operator<<(char *log) {
    append_str(log);
    return *this;
}

logger &logger::operator<<(char ch) {
    this->str.push_back(ch);
    this->str.append(SPLIT);
    return *this;
}

logger::logger(string levelName, uint32_t level) : levelName(std::move(levelName)), level(level) {}

logger &logger::operator<<(const string &string) {
    append_str(string);
    return *this;
}


void logger::append_str(const string &info) {
    if (this->level < LEVEL) {
        return;
    }
    this->str.append(info).append(SPLIT);
}

logger &logger::operator<<(const unordered_set<string> &strs) {
    for (const auto &line : strs) {
        append_str(line);
    }
    return *this;
}

thread_local uint64_t logger::traceId = 0;
udp_logger udp_logger::INSTANCE;
udp_logger::udp_logger() : ctx() {
    worker = new boost::asio::io_context::work(ctx);
    th = new std::thread([=]() { ctx.run(); });
}

udp_logger::~udp_logger() {
    ctx.stop();
    delete worker;
    th->join();
    delete th;
}
void udp_logger::log(const udp_log_server &server, const string &str) {
    ip::udp::endpoint endpoint(ip::make_address_v4(server.ip), server.port);
    auto sc = new ip::udp::socket(ctx, ip::udp::endpoint(ip::udp::v4(), 0));
    auto data = st::mem::pmalloc(str.length());
    copy(str.c_str(), data.first, str.length());
    sc->async_send_to(buffer(data.first, str.length()), endpoint,
                      [=](boost::system::error_code error, std::size_t size) {
                          if (error) {
                              logger::ERROR << "udp log error!" << error.message() << END;
                          }
                          delete sc;
                          st::mem::pfree(data);
                      });
}
std_logger std_logger::INSTANCE;
std_logger::std_logger() {}
void std_logger::log(const string &str, ostream *st) { *st << str; }


udp_log_server apm_logger::UDP_LOG_SERVER;
unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, long>>>> apm_logger::STATISTICS;
boost::asio::io_context apm_logger::IO_CONTEXT;
boost::asio::io_context::work *apm_logger::IO_CONTEXT_WORK = nullptr;
boost::asio::deadline_timer apm_logger::LOG_TIMER(apm_logger::IO_CONTEXT);
std::thread *apm_logger::LOG_THREAD;

apm_logger::apm_logger(const string &name) { this->add_dimension("name", name); }

void apm_logger::start() {
    this->start_time = time::now();
    this->last_step_time = this->start_time;
}

void apm_logger::end() {
    uint64_t cost = time::now() - this->start_time;
    this->add_metric("cost", cost);
    this->add_metric("count", 1);
    boost::property_tree::ptree &dimensions = this->dimensions;
    boost::property_tree::ptree &metrics = this->metrics;
    IO_CONTEXT.post([dimensions, metrics]() {
        string name = dimensions.get<string>("name");
        string dimensionsId = base64::encode(to_json(dimensions));
        for (auto it = metrics.begin(); it != metrics.end(); it++) {
            string metricName = it->first;
            long value = boost::lexical_cast<long>(it->second.data());
            accumulate_metric(STATISTICS[name][dimensionsId][metricName], value);
        }
    });
}


void apm_logger::step(const string &step) {
    uint64_t cost = time::now() - this->last_step_time;
    this->last_step_time = time::now();
    this->add_metric(step + "Cost", cost);
}

void apm_logger::accumulate_metric(unordered_map<string, long> &metric, long value) {
    if (metric.empty()) {
        metric["sum"] = 0;
        metric["min"] = numeric_limits<long>::max();
        metric["max"] = numeric_limits<long>::min();
    }
    metric["sum"] += value;
    metric["min"] = min(value, metric["min"]);
    metric["max"] = max(value, metric["max"]);
}

void apm_logger::perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost) {
    perf(name, std::move(dimensions), cost, 1);
}
void apm_logger::perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost, uint64_t count) {
    IO_CONTEXT.post([=]() {
        boost::property_tree::ptree pt;
        for (auto &dimension : dimensions) {
            pt.put(dimension.first, dimension.second);
        }
        pt.put("name", name);
        string id = base64::encode(to_json(pt));
        accumulate_metric(STATISTICS[name][id]["count"], count);
        accumulate_metric(STATISTICS[name][id]["cost"], cost);
    });
}

void apm_logger::perf(const string &name, unordered_map<string, string> &&dimensions,
                      unordered_map<string, int64_t> &&counts) {
    IO_CONTEXT.post([=]() {
        boost::property_tree::ptree pt;
        for (auto &dimension : dimensions) {
            pt.put(dimension.first, dimension.second);
        }
        pt.put("name", name);
        string id = base64::encode(to_json(pt));
        accumulate_metric(STATISTICS[name][id]["count"], 1);
        for (const auto &count : counts) {
            accumulate_metric(STATISTICS[name][id][count.first], count.second);
        }
    });
}

void apm_logger::enable(const string &udpServerIP, uint16_t udpServerPort) {
    UDP_LOG_SERVER.ip = udpServerIP;
    UDP_LOG_SERVER.port = udpServerPort;
    IO_CONTEXT_WORK = new boost::asio::io_context::work(IO_CONTEXT);
    LOG_THREAD = new std::thread([&]() { IO_CONTEXT.run(); });
    schedule_log();
}
void apm_logger::disable() {
    if (IO_CONTEXT_WORK != nullptr) {
        IO_CONTEXT.stop();
        delete IO_CONTEXT_WORK;
        IO_CONTEXT_WORK = nullptr;
        LOG_THREAD->join();
        delete LOG_THREAD;
    }
}

void apm_logger::schedule_log() {
    LOG_TIMER.expires_from_now(boost::posix_time::milliseconds(65 * 1000 - time::now() % (60 * 1000U)));
    LOG_TIMER.async_wait([=](boost::system::error_code ec) {
        for (auto &it0 : STATISTICS) {
            for (auto &it1 : it0.second) {
                boost::property_tree::ptree finalPT;
                boost::property_tree::ptree t_dimensions = fromJson(base64::decode(it1.first));
                finalPT.insert(finalPT.end(), t_dimensions.begin(), t_dimensions.end());
                long t_count = it1.second["count"]["sum"];
                if (t_count <= 0) {
                    continue;
                }
                for (auto &it2 : it1.second) {
                    if (it2.first != "count") {
                        it2.second["avg"] = it2.second["sum"] / t_count;
                        boost::property_tree::ptree t_metrics;
                        for (auto &it3 : it2.second) {
                            t_metrics.put<long>(it3.first, it3.second);
                        }
                        finalPT.put_child(it2.first, t_metrics);
                    }
                }
                finalPT.put("count", t_count);
                finalPT.put("server_time", time::now_str());
                if (UDP_LOG_SERVER.is_valid()) {
                    udp_logger::INSTANCE.log(UDP_LOG_SERVER, to_json(finalPT));
                }
            }
        }
        STATISTICS.clear();
        schedule_log();
    });
}
apm_logger::~apm_logger() {}

void logger::init(boost::property_tree::ptree &tree) {
    auto logConfig = tree.get_child_optional("log");
    if (logConfig.is_initialized()) {
        logger::LEVEL = logConfig.get().get<int>("level", 1);
        auto rawLogServerConfig = logConfig.get().get_child_optional("raw_log_server");
        auto apmLogServerConfig = logConfig.get().get_child_optional("apm_log_server");
        if (rawLogServerConfig.is_initialized()) {
            logger::UDP_LOG_SERVER.ip = rawLogServerConfig.get().get<string>("ip", "");
            logger::UDP_LOG_SERVER.port = rawLogServerConfig.get().get<uint16_t>("port", 0);
            logger::tag = rawLogServerConfig.get().get<string>("tag", "default");
        }
        if (apmLogServerConfig.is_initialized()) {
            string udpServerIP = apmLogServerConfig.get().get<string>("ip", "");
            uint16_t udpServerPort = apmLogServerConfig.get().get<uint16_t>("port", 0);
            apm_logger::enable(udpServerIP, udpServerPort);
        }
    }
}