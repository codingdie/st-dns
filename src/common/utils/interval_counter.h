
#ifndef INTERVAL_COUNTER_H
#define INTERVAL_COUNTER_H
#include "moment.h"
#include <atomic>
#include <mutex>
#include <utility>
namespace st {
    namespace utils {
        namespace counters {
            class interval {

            private:
                std::atomic<uint64_t> last_interval_time;
                std::atomic<uint64_t> last_record_time;
                std::atomic<uint64_t> interval_count;
                std::atomic<uint64_t> total_count;

            public:
                std::pair<uint64_t, uint64_t> inter() {
                    auto now = st::utils::time::now();
                    uint64_t interval_time = now - last_interval_time;
                    uint64_t count = interval_count.load();
                    last_interval_time = now;
                    interval_count = 0;
                    return std::make_pair<>(interval_time, count);
                };
                interval &operator+=(const uint64_t incr) {
                    last_record_time = st::utils::time::now();
                    if (last_interval_time.load() == 0) {
                        last_interval_time = last_record_time.load();
                    }
                    interval_count += incr;
                    total_count += incr;
                    return *this;
                }
                uint64_t total() const { return total_count.load(); };

                bool is_start() const { return total() > 0; };
                uint64_t get_last_record_time() { return this->last_record_time.load(); };
                interval() : last_interval_time(0), last_record_time(0), interval_count(0), total_count(0){};
                ~interval() = default;
            };
        }// namespace counters
    }    // namespace utils
}// namespace st
#endif
