//
// Created by codingdie on 7/11/22.
//

#ifndef ST_PROXY_MEDIAN_COUNTER_H
#define ST_PROXY_MEDIAN_COUNTER_H
#include <algorithm>
#include <atomic>
#include <queue>
#include <vector>
namespace st {
    namespace utils {
        namespace counters {
            template<typename T>
            class median {
            public:
                median &operator+=(const T num) {
                    if (queue.size() >= max) {
                        queue.pop();
                    }
                    queue.emplace(num);
                    return *this;
                }
                T mid() {
                    auto size = queue.size();
                    if (size == 0) {
                        return 0;
                    }
                    bool ot = size % 2 == 0;
                    std::vector<T> nums;
                    for (auto i = 0; i < queue.size(); i++) {
                        long val = queue.front();
                        queue.pop();
                        queue.push(val);
                        nums.emplace_back(val);
                    }
                    std::sort(nums.begin(), nums.end());
                    if (ot) {
                        return (nums[size / 2 - 1] + nums[size / 2]) / 2;

                    } else {
                        return nums[size / 2];
                    }
                }
                median() = default;
                ;
                ~median() = default;

            private:
                int max = 1000;
                std::queue<T> queue;
            };

        }// namespace counters
    }    // namespace utils
}// namespace st
#endif//ST_PROXY_MEDIAN_COUNTER_H
