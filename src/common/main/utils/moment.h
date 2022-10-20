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
            inline uint64_t now() {
                auto time_now = std::chrono::system_clock::now();
                auto duration_in_ms =
                        std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
                return duration_in_ms.count();
            }
            inline std::string now_str() {
                char buff[20];
                uint64_t nowt = time::now();
                time_t now = nowt / 1000;
                tm *time = localtime(&now);
                strftime(buff, sizeof(buff), "%Y-%m-%dT%H:%M:%S", time);
                std::string result = buff;
                return result + "." + std::to_string(nowt % 1000);
            }
            inline std::string format(uint64_t timestamp) {
                char buff[20];
                uint64_t nowt = timestamp;
                time_t now = nowt / 1000;
                tm *time = localtime(&now);
                strftime(buff, sizeof(buff), "%Y-%m-%dT%H:%M:%S", time);
                std::string result = buff;
                return result + "." + std::to_string(nowt % 1000);
            }
        }// namespace time
    }    // namespace utils
}// namespace st
#endif//ST_TIMEUTILS_H
