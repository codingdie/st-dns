//
// Created by codingdie on 2020/5/19.
//

#include "logger.h"
#include "base64.h"
#include "file.h"
#include "string_utils.h"

#include "pool.h"
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <random>
#include <regex>
#include <thread>
#include <utility>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>

using namespace std;
using namespace st::utils;
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;
BOOST_LOG_GLOBAL_LOGGER_INIT(perf_file_logger, boost::log::sources::severity_logger_mt) {
    boost::log::sources::severity_logger_mt<> lg;
    lg.add_attribute("LOG_TYPE", boost::log::attributes::constant<std::string>("PERF"));
    return lg;
}
BOOST_LOG_GLOBAL_LOGGER_INIT(normal_file_logger, boost::log::sources::severity_logger_mt) {
    boost::log::sources::severity_logger_mt<> lg;
    lg.add_attribute("LOG_TYPE", boost::log::attributes::constant<std::string>("NORMAL"));
    return lg;
}
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
string logger::TAG = "default";
bool logger::INITED = false;


void logger::do_log() {
    if (this->level < LEVEL) {
        this->str.clear();
        return;
    }
    std::lock_guard<std::mutex> lg(logMutex);
    string time = time::now_str();
    string::size_type pos = 0L;
    int lastPos = 0;
    while ((pos = this->str.find('\n', pos)) != std::string::npos) {
        auto line = this->str.substr(lastPos, (pos - lastPos));
        do_log(line);
        pos += 1;
        lastPos = pos;
    }
    auto line = this->str.substr(lastPos, (this->str.length() - lastPos));
    do_log(line);
    this->str.clear();
}
void logger::do_log(string line) const {
    if (INITED) {
        BOOST_LOG(normal_file_logger::get()) << "[" << levelName << "]" << SPLIT << "[" << traceId << "]" << SPLIT << line;
    } else {
        cout << time::now_str() << SPLIT << "[" << levelName << "]" << SPLIT << std::this_thread::get_id() << SPLIT << "[" << traceId << "]" << SPLIT << line << endl;
    }
}


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


logger::logger(string levelName, uint32_t level) : levelName(std::move(levelName)), level(level) {
}

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
unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, uint64_t>>>> apm_logger::STATISTICS;
boost::asio::io_context apm_logger::IO_CONTEXT;
boost::asio::io_context::work *apm_logger::IO_CONTEXT_WORK = nullptr;
boost::asio::deadline_timer apm_logger::LOG_TIMER(apm_logger::IO_CONTEXT);
std::vector<std::thread *> apm_logger::LOG_THREADS;
std::mutex apm_logger::APM_LOCK;


apm_logger::apm_logger(const string &name) { this->add_dimension("name", name); }

void apm_logger::start() {
    this->start_time = time::now();
    this->last_step_time = this->start_time;
}

void apm_logger::end() {
    uint64_t cost = time::now() - this->start_time;
    this->add_metric("cost", cost);
    this->add_metric("count", (uint64_t) 1);
    boost::property_tree::ptree &c_dimensions = this->dimensions;
    boost::property_tree::ptree &c_metrics = this->metrics;
    IO_CONTEXT.post([c_dimensions, c_metrics]() {
        string name = c_dimensions.get<string>("name");
        string dimensionsId = base64::encode(to_json(c_dimensions));
        for (auto it = c_metrics.begin(); it != c_metrics.end(); it++) {
            string metricName = it->first;
            auto value = c_metrics.get<uint64_t>(metricName);
            std::lock_guard<std::mutex> lg(APM_LOCK);
            accumulate_metric(STATISTICS[name][dimensionsId][metricName], value, 1);
        }
    });
}


void apm_logger::step(const string &step) {
    uint64_t cost = time::now() - this->last_step_time;
    this->last_step_time = time::now();
    this->add_metric(step + "Cost", cost);
}

void apm_logger::accumulate_metric(unordered_map<string, uint64_t> &metric, uint64_t value, uint64_t sample) {
    if (metric.empty()) {
        metric["sum"] = (uint64_t) 0;
        metric["min"] = numeric_limits<uint64_t>::max();
        metric["max"] = numeric_limits<uint64_t>::min();
    }
    uint64_t sum = value * sample;
    metric["sum"] += sum;
    metric["min"] = min(value, metric["min"]);
    metric["max"] = max(value, metric["max"]);
}
void apm_logger::perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost, uint64_t sample) {
    perf(name, std::move(dimensions), cost, 1, sample);
}
void apm_logger::perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost) {
    perf(name, std::move(dimensions), cost, 1);
}
void apm_logger::perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost, uint64_t count,
                      uint64_t sample) {
    if (is_sample(sample)) {
        IO_CONTEXT.post([=]() {
            boost::property_tree::ptree pt;
            for (auto &dimension : dimensions) {
                pt.put(dimension.first, dimension.second);
            }
            pt.put("name", name);
            string id = base64::encode(to_json(pt));
            std::lock_guard<std::mutex> lg(APM_LOCK);
            accumulate_metric(STATISTICS[name][id]["count"], count, sample);
            accumulate_metric(STATISTICS[name][id]["cost"], cost, sample);
        });
    }
}
bool apm_logger::is_sample(uint64_t sample) {
    static uint64_t range = 1000;
    static uniform_int_distribution<unsigned short> random_range(0, range - 1);//随机数分布对象
    static default_random_engine random_engine(time::now());
    auto scale = range / sample;
    return random_range(random_engine) / scale == 0;
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
        std::lock_guard<std::mutex> lg(APM_LOCK);
        accumulate_metric(STATISTICS[name][id]["count"], 1, 1);
        for (const auto &count : counts) {
            accumulate_metric(STATISTICS[name][id][count.first], count.second, 1);
        }
    });
}

void apm_logger::init() {
    IO_CONTEXT_WORK = new boost::asio::io_context::work(IO_CONTEXT);
    unsigned int cpu_count = std::thread::hardware_concurrency();
    for (auto i = 0; i < cpu_count; i++) {
        auto *th = new std::thread([&]() { IO_CONTEXT.run(); });
        LOG_THREADS.emplace_back(th);
    }
    schedule_log();
}
void apm_logger::disable() {
    if (IO_CONTEXT_WORK != nullptr) {
        IO_CONTEXT.stop();
        delete IO_CONTEXT_WORK;
        IO_CONTEXT_WORK = nullptr;
        for (thread *th : LOG_THREADS) {
            th->join();
            delete th;
        }
    }
    report_apm_log_local();
}

void apm_logger::schedule_log() {
    LOG_TIMER.expires_from_now(boost::posix_time::milliseconds(60 * 1000));
    LOG_TIMER.async_wait([=](boost::system::error_code ec) {
        schedule_log();
        report_apm_log_local();
    });
}
void apm_logger::report_apm_log_local() {
    unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, uint64_t>>>>
            metric_duplicate;
    auto begin = time::now();
    {
        lock_guard<mutex> lg(APM_LOCK);
        metric_duplicate = STATISTICS;
        STATISTICS.clear();
    }
    if (metric_duplicate.empty()) {
        return;
    }
    auto folder = "/tmp/st/perf/";
    auto filename = folder + time::now_str("%Y-%m-%d-%H-%M") + ".perf." + strutils::uuid();
    file::create_if_not_exits(filename);
    file::limit_file_cnt(folder, 200);
    ofstream fs(filename);
    if (fs) {
        for (auto &it0 : metric_duplicate) {
            for (auto &it1 : it0.second) {
                boost::property_tree::ptree finalPT;
                boost::property_tree::ptree t_dimensions = fromJson(base64::decode(it1.first));
                finalPT.insert(finalPT.end(), t_dimensions.begin(), t_dimensions.end());
                uint64_t t_count = it1.second["count"]["sum"];
                if (t_count <= 0) {
                    continue;
                }
                for (auto &it2 : it1.second) {
                    if (it2.first != "count") {
                        it2.second["avg"] = it2.second["sum"] / t_count;
                        boost::property_tree::ptree t_metrics;
                        for (auto &it3 : it2.second) {
                            t_metrics.put<uint64_t>(it3.first, it3.second);
                        }
                        finalPT.put_child(it2.first, t_metrics);
                    }
                }
                finalPT.put("count", t_count);
                finalPT.put("server_time", time::now_str());
                fs << to_json(finalPT) << "\n";
            }
        }
        fs.flush();
        fs.close();
    }
    uint64_t cost = time::now() - begin;
    logger::INFO << "apm log report at" << time::now_str() << "cost" << cost << END;
}
apm_logger::~apm_logger() {}

void logger::disable() {
    apm_logger::disable();
    boost::shared_ptr<logging::core> core = logging::core::get();
    core->flush();
    core->remove_all_sinks();
    core->reset_filter();
}

void logger::init(boost::property_tree::ptree &tree) {
    auto logConfig = tree.get_child_optional("log");
    if (logConfig.is_initialized()) {
        logger::LEVEL = logConfig.get().get<int>("level", 1);
        logger::TAG = logConfig.get().get<string>("tag", "default");
    }
    apm_logger::init();
    boost::shared_ptr<logging::core> core = logging::core::get();
    core->remove_all_sinks();
    core->reset_filter();
    typedef sinks::asynchronous_sink<sinks::text_file_backend> sink_t;
    boost::shared_ptr<sinks::text_file_backend> backend =
            boost::make_shared<sinks::text_file_backend>(
                    keywords::file_name = "/tmp/st/" + logger::TAG + ".log",
                    keywords::target_file_name = logger::TAG + ".log.%Y%m%d%H-%N",
                    keywords::rotation_size = 1024 * 1024,
                    keywords::time_based_rotation = sinks::file::rotation_at_time_interval(boost::posix_time::hours(1)));
    boost::shared_ptr<sink_t> sink(new sink_t(backend));
    sink->locked_backend()->set_file_collector(sinks::file::make_collector(
            keywords::target = "/tmp/st",
            keywords::max_size = 16 * 1024 * 1024,
            keywords::max_files = 16));
    sink->locked_backend()->scan_for_files();
    sink->locked_backend()->enable_final_rotation(false);
    sink->set_formatter(expr::stream
                        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                        << " " << expr::smessage);
    core->add_sink(sink);
    logging::add_common_attributes();
    logger::INITED = true;
}