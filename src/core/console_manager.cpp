//
// Created by codingdie on 2025/12/19.
//

#include "console_manager.h"

#include <cstdio>

st::dns::console_manager &st::dns::console_manager::uniq() {
    static console_manager instance;
    return instance;
}

void st::dns::console_manager::init(const std::string &ip, uint16_t port) {
    shutdown();
    console = new st::console::udp_console(ip, port);
    console->desc.add_options()("domain", boost::program_options::value<string>()->default_value(""), "domain");
    console->desc.add_options()("ip", boost::program_options::value<string>()->default_value(""), "ip");
    console->impl = [this](const vector<string> &commands, const boost::program_options::variables_map &options) {
        auto command = utils::strutils::join(commands, " ");
        std::pair<bool, std::string> result = make_pair(false, "not invalid command");
        string ip = options["ip"].as<string>();
        if (command == "dns resolve") {
            if (options.count("domain")) {
                auto domain = options["domain"].as<string>();
                if (!domain.empty()) {
                    auto record = dns_record_manager::uniq().resolve(domain);
                    result = make_pair(true, record.serialize());
                }
            }
        } else if (command == "dns reverse resolve") {
            if (!ip.empty()) {
                auto record = dns_record_manager::uniq().reverse_resolve(st::utils::ipv4::str_to_ip(ip));
                result = make_pair(true, join(record.domains(), ","));
            }
        } else if (command == "dns record get") {
            if (options.count("domain")) {
                auto domain = options["domain"].as<string>();
                if (!domain.empty()) {
                    auto records = dns_record_manager::uniq().get_dns_record_list(domain);
                    vector<string> strs(records.size());
                    std::transform(records.begin(), records.end(), strs.begin(), [](const dns_record &item) { return item.serialize(); });
                    result = make_pair(true, strutils::join(strs, "\n"));
                }
            }
        } else if (command == "dns record dump") {
            result = make_pair(true, dns_record_manager::uniq().dump());
        } else if (command == "dns record remove") {
            if (options.count("domain")) {
                auto domain = options["domain"].as<string>();
                if (!domain.empty()) {
                    dns_record_manager::uniq().remove(domain);
                    result = make_pair(true, "");
                }
            }
        } else if (command == "dns record clear") {
            dns_record_manager::uniq().clear();
            result = make_pair(true, "");
        } else if (command == "dns record analyse") {
            result = make_pair(true, dns_record_manager::uniq().stats().serialize());
        } else if (command == "ip area") {
            result = make_pair(true, areaip::manager::uniq().get_area(st::utils::ipv4::str_to_ip(ip)));
        } else if (command == "dns auto lan ip") {
            vector<string> lines;
            for (const auto &server : st::dns::config::INSTANCE.servers) {
                if (server->type == "UDP" &&
                    find(server->areas.begin(), server->areas.end(), "LAN") != server->areas.end()) {
                    lines.emplace_back(server->ip + ":" + to_string(server->port));
                }
            }
            if (lines.empty()) {
                result = make_pair(true, "未找到 LAN UDP 上游");
            } else {
                result = make_pair(true, strutils::join(lines, "\n"));
            }
        } else if (command == "dns queue list") {
            if (this->sync_queue == nullptr) {
                result = make_pair(false, "sync queue not initialized");
            } else {
                auto tasks = this->sync_queue->all();
                auto now = time::now();
                vector<string> lines;
                lines.reserve(tasks.size() + 1);
                string header = "domain                           server                   queued_at                    elapsed     status";
                lines.emplace_back(header);
                for (const auto &task : tasks) {
                    auto elapsed = now - task.create_time;
                    auto status_str = task.status == st::task::PENDING ? "pending" : "running";
                    auto *server = task.in.second;
                    string server_str = server != nullptr ? server->id() : "unknown";
                    char line[256];
                    snprintf(line, sizeof(line), "%-32s %-24s %-26s %4llums   %s",
                             task.in.first.c_str(),
                             server_str.c_str(),
                             time::format(task.create_time).c_str(),
                             (unsigned long long) elapsed,
                             status_str);
                    lines.emplace_back(line);
                }
                result = make_pair(true, strutils::join(lines, "\n"));
            }
        }
        return result;
    };
}

void st::dns::console_manager::set_sync_queue(sync_record_task_queue *queue) {
    sync_queue = queue;
}

void st::dns::console_manager::start() {
    if (console != nullptr) {
        console->start();
    }
}

void st::dns::console_manager::shutdown() {
    if (console != nullptr) {
        console->stop();
        delete console;
        console = nullptr;
    }
}

st::dns::console_manager::~console_manager() {
    shutdown();
}
