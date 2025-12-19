//
// Created by codingdie on 2025/12/19.
//

#include "console_manager.h"

st::dns::console_manager &st::dns::console_manager::uniq() {
    static console_manager instance;
    return instance;
}

void st::dns::console_manager::init(const std::string &ip, uint16_t port) {
    console = new st::console::udp_console(ip, port);
    console->desc.add_options()("domain", boost::program_options::value<string>()->default_value(""), "domain");
    console->desc.add_options()("ip", boost::program_options::value<string>()->default_value(""), "ip");
    console->impl = [](const vector<string> &commands, const boost::program_options::variables_map &options) {
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
        } else if (command == "dns queue list") {
            // Note: This requires access to sync_remote_record_task_queue, which is in dns_server
            // For now, leave it out or pass it somehow
            result = make_pair(false, "not implemented in singleton");
        }
        return result;
    };
}

void st::dns::console_manager::start() {
    if (console != nullptr) {
        console->start();
    }
}

st::dns::console_manager::~console_manager() {
    if (console != nullptr) {
        delete console;
    }
}