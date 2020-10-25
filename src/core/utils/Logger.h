//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_LOGGER_H
#define ST_LOGGER_H

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>

#define END Logger::MASK::ENDL;

static const char *const SPLIT = " ";
using namespace std;
namespace st {
    namespace utils {
        class Logger {
        private:
            uint32_t level = 0;
            string tag;
            string str;

            void appendStr(const string &info);

            static void getTime(string &timeStr);

        public:
            static thread_local uint64_t traceId;
            enum MASK {
                ENDL
            };
            static thread_local Logger TRACE;
            static thread_local Logger DEBUG;
            static thread_local Logger INFO;
            static thread_local Logger ERROR;
            static uint32_t LEVEL;

            explicit Logger(string tag, uint32_t level);


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

            ostream *getStream();

            void doLog(const string &time, ostream &st, const string &line);
        };
    }// namespace utils
}// namespace st

#endif//ST_LOGGER_H
