//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_LOGGER_H
#define ST_LOGGER_H

#include <string>
#include <chrono>
#include <iostream>
#include <set>
#include <mutex>

#define END Logger::MASK::ENDL;


static std::string _CutParenthesesNTail(std::string &&prettyFuncon) {
    auto pos = prettyFuncon.find('(');
    if (pos != std::string::npos)
        prettyFuncon.erase(prettyFuncon.begin() + pos, prettyFuncon.end());
    auto pos2 = prettyFuncon.find(' ');
    if (pos2 != std::string::npos)
        prettyFuncon.erase(prettyFuncon.begin(), prettyFuncon.begin() + pos2 + 1);
    return std::move(prettyFuncon);
}

#define __STR_FUNCTION__ _CutParenthesesNTail(std::string(__PRETTY_FUNCTION__))


static const char *const SPLIT = " ";
using namespace std;
namespace st {
    namespace utils {
        class Logger {
        private:
            string tag;
            string str;

            void appendStr(const string &info);

            static void getTime(string &timeStr);

        public:
            static thread_local uint64_t traceId;
            enum MASK {
                ENDL
            };
            static thread_local Logger INFO;

            static thread_local Logger ERROR;

            static thread_local Logger DEBUG;

            explicit Logger(string tag);


            void doLog();

            Logger &operator<<(const char *log);

            Logger &operator<<(char ch);

            Logger &operator<<(const string &string);

            Logger &operator<<(char *log);

            Logger &operator<<(const set<string> &strs);

            template<typename A> Logger &operator<<(const A &str1) {
                if (typeid(str1) == typeid(MASK) && str1 == ENDL) {
                    doLog();
                } else {
                    this->appendStr(to_string(str1));
                }
                return *this;
            }

            ostream *getStream();
        };
    }
}

#endif //ST_LOGGER_H
