//
// Created by 徐芃 on 2020/5/30.
//

#ifndef ST_DNS_TIMEUTILS_H
#define ST_DNS_TIMEUTILS_H
namespace st {
    namespace utils {
        namespace time {
            static long now() {
                auto time_now = std::chrono::system_clock::now();
                auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        time_now.time_since_epoch());
                return duration_in_ms.count();
            }
        }
    }
}
#endif //ST_DNS_TIMEUTILS_H
