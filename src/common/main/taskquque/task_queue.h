//
// Created by codingdie on 11/25/23.
//

#ifndef ST_TASK_QUEUE_H
#define ST_TASK_QUEUE_H
#include "st.h"
#include <functional>
#include <unordered_map>
#include <utility>
namespace st {
    namespace task {
        enum task_status { PENDING, RUNNING };
        template<typename input>
        class priority_task {
        public:
            input in;
            uint64_t priority = 0;
            uint64_t id = 0;
            string pk;
            uint64_t create_time = 0;
            task_status status;
            explicit priority_task(const input &in) : priority_task(in, 0, ""){};
            priority_task(const input &in, uint64_t priority) : priority_task(in, priority, ""){};
            priority_task(const input &in, uint64_t priority, string task_id)
                : in(in), priority(priority), pk(std::move(task_id)), create_time(time::now()), status(PENDING) {
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
        class task_comparator {
        public:
            bool operator()(const pair<uint64_t, uint64_t> &a, const pair<uint64_t, uint64_t> &b) const {
                return a.second < b.second;
            }
        };
        template<typename input>
        class queue {
        private:
            std::string name;
            io_context ic;
            io_context::work *iw = nullptr;
            thread th;
            boost::asio::deadline_timer generate_key_timer;
            boost::asio::deadline_timer schedule_timer;
            std::mutex mutex;
            std::function<void(const st::task::priority_task<input> &)> executor;
            volatile double key_count = 0;
            volatile double max_qps = 1;
            uint32_t max_running = 1;
            uint32_t running = 0;

            std::unordered_map<uint64_t, st::task::priority_task<input>> tasks;
            std::priority_queue<pair<uint64_t, uint64_t>, vector<pair<uint64_t, uint64_t>>, task_comparator> p_queue;
            std::unordered_set<std::string> task_pks;

            void schedule_generate_key() {
                uint16_t duration = 50;
                generate_key_timer.expires_from_now(boost::posix_time::milliseconds(duration));
                generate_key_timer.async_wait([this, duration](error_code ec) {
                    key_count += max_qps * duration / 1000;
                    key_count = min(max_qps * 1.0, (double) key_count);
                    schedule_generate_key();
                });
            }

            void schedule_dispatch_task() {
                boost::asio::post(ic, [this]() {
                    std::lock_guard<std::mutex> lg(mutex);
                    if (key_count >= 1 && !p_queue.empty() && running < max_running) {
                        pair<uint64_t, uint64_t> id_score = p_queue.top();
                        st::task::priority_task<input> &task = tasks.at(id_score.first);
                        task.status = RUNNING;
                        p_queue.pop();
                        key_count--;
                        running++;
                        apm_logger::perf("st-task-queue-stats",
                                         {{"queue_name", name}, {"priority", to_string(task.priority)}},
                                         {{"heap", p_queue.size()}, {"running", running}});
                        boost::asio::post(ic, [this, task]() { executor(task); });
                    }
                });

                schedule_timer.expires_from_now(boost::posix_time::milliseconds(50));
                schedule_timer.async_wait([this](error_code ec) { schedule_dispatch_task(); });
            }

        public:
            explicit queue(string name, uint32_t speed, uint32_t max_running,
                           const std::function<void(st::task::priority_task<input>)> &executor)
                : name(std::move(name)), ic(), iw(new io_context::work(ic)), th([this]() { ic.run(); }),
                  generate_key_timer(ic), schedule_timer(ic), executor(executor), max_qps(speed),
                  max_running(max_running), running(0) {
                schedule_generate_key();
                schedule_dispatch_task();
            };

            bool submit(const st::task::priority_task<input> &task) {
                std::lock_guard<std::mutex> lg(mutex);
                if (task.pk.empty() || task_pks.emplace(task.pk).second) {
                    tasks.emplace(make_pair(task.id, task));
                    p_queue.push(make_pair(task.id, task.priority));
                    return true;
                } else {
                    return false;
                }
            }
            void complete(const st::task::priority_task<input> &task) {
                std::lock_guard<std::mutex> lg(mutex);
                task_pks.erase(task.pk);
                tasks.erase(task.id);
                running--;
            }

            vector<st::task::priority_task<input>> all() {
                std::lock_guard<std::mutex> lg(mutex);
                vector<st::task::priority_task<input>> task_res;
                for (const auto &pair : tasks) {
                    task_res.emplace_back(pair.second);
                }
                return task_res;
            }
            uint32_t size() {
                std::lock_guard<std::mutex> lg(mutex);
                return tasks.size();
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
