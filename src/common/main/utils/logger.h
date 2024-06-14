//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_LOGGER_H
#define ST_LOGGER_H

#include "asio.h"
#include "moment.h"
#include <atomic>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions/keyword.hpp>

BOOST_LOG_GLOBAL_LOGGER(perf_file_logger, boost::log::sources::severity_logger_mt<>)
BOOST_LOG_GLOBAL_LOGGER(normal_file_logger, boost::log::sources::severity_logger_mt<>)
BOOST_LOG_ATTRIBUTE_KEYWORD(log_type_attr, "LOG_TYPE", std::string)

static const char *const SPLIT = " ";
using namespace std;
namespace st {
    namespace utils {

        class logger {
        private:
            string levelName;
            uint32_t level = 0;
            string str;
            static thread_local boost::asio::io_context ctxThreadLocal;
            void append_str(const string &info);
            void do_log();
            void do_log(string line) const;

        public:
            void static init(boost::property_tree::ptree &config);
            void static disable();
            static thread_local uint64_t traceId;
            enum MASK { ENDL };
            static thread_local logger DEBUG;
            static thread_local logger WARN;
            static thread_local logger INFO;
            static thread_local logger ERROR;

            static uint32_t LEVEL;
            static string TAG;
            static bool INITED;

            explicit logger(string levelName, uint32_t level);


            logger &operator<<(const char *log);

            logger &operator<<(char ch);

            logger &operator<<(const string &string);

            logger &operator<<(char *log);

            logger &operator<<(const unordered_set<string> &strs);

            template<typename A>
            logger &operator<<(const A &str1) {
                if (typeid(str1) == typeid(MASK) && str1 == ENDL) {
                    do_log();
                } else {
                    this->append_str(to_string(str1));
                }
                return *this;
            }
        };
#define END st::utils::logger::MASK::ENDL
        class apm_logger {
        public:
            static void init();
            static void disable();
            static void perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost,
                             uint64_t count, uint64_t sample);
            static void perf(const string &name, unordered_map<string, string> &&dimensions,
                             unordered_map<string, int64_t> &&counts);
            static void perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost,
                             uint64_t sample);

            static void perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost);
            explicit apm_logger(const string &name);
            virtual ~apm_logger();
            void start();
            void step(const string &step);
            void end();
            template<class V>
            void add_dimension(const string name, const V &value) {
                this->dimensions.put<V>(name, value);
            }
            void add_metric(const string &name, const uint64_t &value) { this->metrics.put<uint64_t>(name, value); }

        private:
            static unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, uint64_t>>>>
                    STATISTICS;
            static boost::asio::deadline_timer LOG_TIMER;
            static boost::asio::io_context IO_CONTEXT;
            static std::mutex APM_LOCK;
            static boost::asio::io_context::work *IO_CONTEXT_WORK;
            static std::vector<std::thread *> LOG_THREADS;
            static void schedule_log();
            static void accumulate_metric(unordered_map<string, uint64_t> &metric, uint64_t value, uint64_t sample);

            boost::property_tree::ptree dimensions;
            boost::property_tree::ptree metrics;
            uint64_t start_time;
            uint64_t last_step_time;
            static bool is_sample(uint64_t sample);
            static void report_apm_log_local();
        };
    }// namespace utils
}// namespace st

#endif//ST_LOGGER_H
