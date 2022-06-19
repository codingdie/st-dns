
#ifndef INTERVAL_COUNTER_H
#define INTERVAL_COUNTER_H
#include "time.h"
#include <atomic>
#include <mutex>
#include <utility>

class interval_counter {

public:
    std::atomic<uint64_t> lastIntervalTime;
    std::atomic<uint64_t> lastRecordTime;
    std::atomic<uint64_t> startTime;
    std::atomic<uint64_t> intervalCount;
    std::atomic<uint64_t> totalCount;

    std::pair<uint64_t, uint64_t> interval() {
        long now = st::utils::time::now();
        uint64_t intervalTime = now - lastIntervalTime;
        uint64_t count = intervalCount.load();
        lastIntervalTime = now;
        intervalCount = 0;
        return std::make_pair<>(intervalTime, count);
    };
    std::pair<uint64_t, uint64_t> total() const {
        long now = st::utils::time::now();
        return std::make_pair<>(now - startTime.load(), totalCount.load());
    };
    interval_counter &operator+=(const uint64_t incr) {
        lastRecordTime = st::utils::time::now();
        if (lastIntervalTime.load() == 0) {
            lastIntervalTime = lastRecordTime.load();
            startTime = lastRecordTime.load();
        }
        intervalCount += incr;
        totalCount += incr;
        return *this;
    }
    bool isStart() { return totalCount.load() > 0; };
    uint64_t getLastRecordTime() { return this->lastRecordTime.load(); };
    interval_counter() : lastIntervalTime(0), lastRecordTime(0), startTime(0), intervalCount(0), totalCount(0){};
    ~interval_counter() {}
};

#endif
