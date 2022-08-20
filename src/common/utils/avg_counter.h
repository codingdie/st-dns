//
// Created by codingdie on 7/11/22.
//

#ifndef ST_PROXY_AVG_COUNTER_H
#define ST_PROXY_AVG_COUNTER_H
#include <algorithm>
#include <atomic>
#include <queue>
#include <vector>
namespace st {
    namespace utils {
        namespace counters {
            template<typename T>
            class average {
            public:
                average &operator+=(const T value) {
                    total += value;
                    num++;
                    return *this;
                }
                T avg() { return total.load() / num.load(); };
                average() : total(0), num(0){};
                ~average() = default;

            private:
                std::atomic<T> total;
                std::atomic<uint64_t> num;
            };

        }// namespace counters
    }    // namespace utils
}// namespace st
#endif//ST_PROXY_AVG_COUNTER_H
