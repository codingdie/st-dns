//
// Created by codingdie on 11/25/23.
//

#ifndef ST_TASK_QUEUE_H
#define ST_TASK_QUEUE_H
#include "st.h"
#include <functional>
namespace st {
    namespace task {

        template<typename input>
        class priority_task {

        public:
            input in;
            uint32_t priority = 0;
            uint64_t id = 0;
            string pk = nullptr;
            uint64_t create_time = 0;
            explicit priority_task(const input &in) : priority_task(in, 0, ""){};
            priority_task(const input &in, uint32_t priority) : priority_task(in, priority, ""){};
            priority_task(const input &in, uint32_t priority, const string &task_id)
                : in(in), priority(priority), pk(task_id), create_time(time::now()) {
                static std::atomic<uint64_t> id_generator(time::now());
                id = id_generator++;
            };
            bool operator<(const priority_task<input> &rhs) const {
                if (priority == rhs.priority) {
                    return id > rhs.id;
                }
                return priority < rhs.priority;
            };
            const input &get_input() const { return in; }
        };
        template<typename input>
        class queue {
        private:
            std::priority_queue<st::task::priority_task<input>, vector<st::task::priority_task<input>>,
                                std::less<priority_task<input>>>
                    i_queue;
            std::string name;
            io_context ic;
            io_context::work *iw = nullptr;
            thread th;
            boost::asio::deadline_timer generate_key_timer;
            boost::asio::deadline_timer schedule_timer;

            std::mutex mutex;
            std::function<void(const st::task::priority_task<input> &)> executor;
            volatile double key_count = 0;
            volatile double speed = 1;
            volatile atomic_uint32_t running;
            uint32_t max_running = 1;
            std::unordered_set<std::string> task_keys;

            void schedule_generate_key() {
                uint16_t duration = 50;
                generate_key_timer.expires_from_now(boost::posix_time::milliseconds(duration));
                generate_key_timer.async_wait([this, duration](error_code ec) {
                    key_count += speed * duration / 1000;
                    key_count = min(speed * 1.0, (double) key_count);
                    schedule_generate_key();
                });
            }

            void schedule_dispatch_task() {
                boost::asio::post(ic, [this]() {
                    std::lock_guard<std::mutex> lg(mutex);
                    if (key_count >= 1 && !i_queue.empty() && running < max_running) {
                        task::priority_task<input> task = i_queue.top();
                        i_queue.pop();
                        key_count--;
                        running++;
                        apm_logger::perf("st-task-queue-stats",
                                         {{"queue_name", name}, {"priority", to_string(task.priority)}},
                                         {{"heap", i_queue.size()}, {"running", running}});
                        boost::asio::post(ic, [this, task]() { executor(task); });
                    }
                });

                schedule_timer.expires_from_now(boost::posix_time::milliseconds(50));
                schedule_timer.async_wait([this](error_code ec) { schedule_dispatch_task(); });
            }

        public:
            explicit queue(const string &name, uint32_t speed, uint32_t max_running,
                           const std::function<void(st::task::priority_task<input>)> &executor)
                : name(name), ic(), iw(new io_context::work(ic)), th([this]() { ic.run(); }), generate_key_timer(ic),
                  schedule_timer(ic), executor(executor), speed(speed), running(0), max_running(max_running) {
                schedule_generate_key();
                schedule_dispatch_task();
            };

            bool submit(const st::task::priority_task<input> &task) {
                std::lock_guard<std::mutex> lg(mutex);
                if (task.pk.empty() || task_keys.emplace(task.pk).second) {
                    i_queue.push(task);
                    return true;
                } else {
                    return false;
                }
            }
            void complete(const st::task::priority_task<input> &task) {
                std::lock_guard<std::mutex> lg(mutex);
                task_keys.erase(task.pk);
                running--;
            }
            ~queue() {
                ic.stop();
                delete iw;
                th.join();
            }
        };
    }// namespace task
}// namespace st
#endif//ST_TASK_QUEUE_H
