//
// Created by codingdie on 2025/12/19.
//

#ifndef ST_CONSOLE_MANAGER_H
#define ST_CONSOLE_MANAGER_H

#include "st.h"
#include "console/udp_console.h"
#include "dns_record_manager.h"
#include "config.h"
#include "utils/area_ip.h"
#include "command/proxy_command.h"
#include "taskquque/task_queue.h"

namespace st {
    namespace dns {
        using sync_record_task_queue = st::task::queue<pair<string, remote_dns_server *>>;
        class console_manager {
        private:
            console_manager() = default;
            st::console::udp_console *console = nullptr;
            sync_record_task_queue *sync_queue = nullptr;

        public:
            static console_manager &uniq();

            void init(const std::string &ip, uint16_t port);

            void set_sync_queue(sync_record_task_queue *queue);

            void start();

            void shutdown();

            ~console_manager();
        };
    }// namespace dns
}// namespace st

#endif//ST_CONSOLE_MANAGER_H
