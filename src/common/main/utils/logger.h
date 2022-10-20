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

static const char *const SPLIT = " ";
using namespace std;
namespace st {
    namespace utils {
        class udp_log_server {
        public:
            string ip;
            uint16_t port = 0;
            bool is_valid() const;
        };
        class udp_logger {
        public:
            static udp_logger INSTANCE;
            udp_logger();
            ~udp_logger();
            void log(const udp_log_server &server, const string &str);

        private:
            boost::asio::io_context ctx;
            boost::asio::io_context::work *worker;
            std::thread *th;
        };
        class std_logger {
        public:
            string tag;
            static std_logger INSTANCE;
            std_logger();
            static void log(const string &str, ostream *st);
        };
        class logger {
        private:
            string levelName;
            uint32_t level = 0;
            string str;
            static thread_local boost::asio::io_context ctxThreadLocal;
            void append_str(const string &info);
            void do_log(const string &time, ostream &st, const string &line);
            ostream *get_std();
            void do_log();

        public:
            void static init(boost::property_tree::ptree &config);
            static thread_local uint64_t traceId;
            enum MASK { ENDL };
            static thread_local logger DEBUG;
            static thread_local logger WARN;
            static thread_local logger INFO;
            static thread_local logger ERROR;

            static uint32_t LEVEL;
            static udp_log_server UDP_LOG_SERVER;
            static string tag;

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
            static void enable(const string &udpServerIP, uint16_t udpServerPort);
            static void disable();
            static void perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost,
                             uint64_t count);
            static void perf(const string &name, unordered_map<string, string> &&dimensions,
                             unordered_map<string, int64_t> &&counts);

            static void perf(const string &name, unordered_map<string, string> &&dimensions, uint64_t cost);
            apm_logger(const string &name);
            void start();
            void step(const string &step);
            void end();
            template<class V>
            void add_dimension(const string name, const V &value) {
                this->dimensions.put<V>(name, value);
            }
            void add_metric(const string &name, const long &value) { this->metrics.put<long>(name, value); }

        private:
            static unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, long>>>>
                    STATISTICS;
            static udp_log_server UDP_LOG_SERVER;
            static boost::asio::deadline_timer LOG_TIMER;
            static boost::asio::io_context IO_CONTEXT;
            static boost::asio::io_context::work *IO_CONTEXT_WORK;
            static std::thread *LOG_THREAD;
            static void schedule_log();
            static void accumulate_metric(unordered_map<string, long> &metric, long value);

            boost::property_tree::ptree dimensions;
            boost::property_tree::ptree metrics;
            uint64_t start_time;
            uint64_t last_step_time;
        };
    }// namespace utils
}// namespace st

#endif//ST_LOGGER_H
