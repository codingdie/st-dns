//
// Created by 徐芃 on 2020/5/30.
//

#ifndef ST_TIMEUTILS_H
#define ST_TIMEUTILS_H

#include <chrono>
#include <string>

namespace st {
    namespace utils {
        namespace time {
            static uint64_t now() {
                auto time_now = std::chrono::system_clock::now();
                auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        time_now.time_since_epoch());
                return duration_in_ms.count();
            }
            static std::string nowStr() {
                char buff[20];
                time_t now = time::now() / 1000;
                tm *time = localtime(&now);
                strftime(buff, sizeof(buff), "%Y-%m-%dT%H:%M:%S", time);
                return buff;
            }
        }// namespace time
    }    // namespace utils
}// namespace st
#endif//ST_TIMEUTILS_H
