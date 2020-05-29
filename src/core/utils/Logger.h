//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_LOGGER_H
#define ST_DNS_LOGGER_H

#include <string>
#include <chrono>
#include <iostream>

#define END Logger::MASK::ENDL;

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
            static thread_local Logger INFO;

            static thread_local Logger ERROR;

            explicit Logger(string tag);

            void doLog(const string &level, const string &info);

            Logger &operator<<(const char *log);

            Logger &operator<<(const string &string);


            template<typename A>
            Logger &operator<<(const A &str1) {

                if (typeid(A) == typeid(MASK) && str1 == ENDL) {
                    doLog(this->tag, this->str);
                } else {
                    this->appendStr(to_string(str1));
                }
                return *this;

            }

            enum MASK {
                ENDL
            };


        };

    }
}

#endif //ST_DNS_LOGGER_H
