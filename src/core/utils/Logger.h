//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_LOGGER_H
#define ST_LOGGER_H

#include "TimeUtils.h"
#include "asio/STUtils.h"
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <atomic>

static const char *const SPLIT = " ";
using namespace std;
namespace st {
    namespace utils {
        class UDPLogServer {
        public:
            string ip = "";
            uint16_t port = 0;
            bool isValid();
        };
        class UDPLogger {
        public:
            static UDPLogger INSTANCE;
            UDPLogger();
            ~UDPLogger();
            void log(const UDPLogServer &server, const string str);

        private:
            boost::asio::io_context ctx;
            boost::asio::io_context::work *worker;
            std::thread *th;
        };
        class STDLogger {
        public:
            string tag = "";
            static STDLogger INSTANCE;
            STDLogger();
            void log(const string str, ostream *st);
        };
        class Logger {
        private:
            string levelName;
            uint32_t level = 0;
            string str;
            void appendStr(const string &info);
            void doLog(const string &time, ostream &st, const string &line);
            ostream *getSTD();
            bool enableUDPLogger();

        public:
            void static init(boost::property_tree::ptree &config);
            static thread_local uint64_t traceId;
            enum MASK { ENDL };
            static thread_local Logger DEBUG;
            static thread_local Logger WARN;
            static thread_local Logger INFO;
            static thread_local Logger ERROR;
            static thread_local boost::asio::io_context ctxThreadLocal;

            static uint32_t LEVEL;
            static UDPLogServer UDP_LOG_SERVER;
            static string tag;

            explicit Logger(string levelName, uint32_t level);


            void doLog();

            Logger &operator<<(const char *log);

            Logger &operator<<(char ch);

            Logger &operator<<(const string &string);

            Logger &operator<<(char *log);

            Logger &operator<<(const unordered_set<string> &strs);

            template<typename A>
            Logger &operator<<(const A &str1) {
                if (typeid(str1) == typeid(MASK) && str1 == ENDL) {
                    doLog();
                } else {
                    this->appendStr(to_string(str1));
                }
                return *this;
            }
        };
#define END st::utils::Logger::MASK::ENDL;
        class APMLogger {
        public:
            static void enable(const string udpServerIP,const uint16_t udpServerPort);
            static void disable();
            static void perf(string name, unordered_map<string, string> &&dimensions, long cost);
            APMLogger(const string name);
            void start();
            void step(const string step);
            void end();
            template<class V>
            void addDimension(const string name, const V &value) {
                this->dimensions.put<V>(name, value);
            }
            void addMetric(const string name, const long &value) {
                this->metrics.put<long>(name, value);
            }

        private:
            static unordered_map<string, unordered_map<string, unordered_map<string, unordered_map<string, long>>>> STATISTICS;
            static std::mutex APM_STATISTICS_MUTEX;
            static UDPLogServer UDP_LOG_SERVER;
            static boost::asio::deadline_timer LOG_TIMMER;
            static boost::asio::io_context IO_CONTEXT;
            static boost::asio::io_context::work *IO_CONTEXT_WORK;
            static std::thread *LOG_THREAD;
            static void scheduleLog();
            static void accumulateMetric(unordered_map<string, long> &metric, long value);

            boost::property_tree::ptree dimensions;
            boost::property_tree::ptree metrics;
            uint64_t startTime;
            uint64_t lastStepTime;
        };
    }// namespace utils
}// namespace st

#endif//ST_LOGGER_H
