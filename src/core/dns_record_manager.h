//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_RECORD_MANAGER_H
#define ST_DNS_DNS_RECORD_MANAGER_H

#include "st.h"

#include <iostream>
#include <mutex>

#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "kv/disk_kv.h"
#include "protocol/message.pb.h"
using namespace std;

class dns_ip_record {
public:
    uint32_t ip = 0;
    bool match_area = false;
    bool forbid = false;
    bool expire = false;
    uint64_t expire_time = 0;
    string server;
    uint8_t server_order;
    static bool compare(dns_ip_record &a, dns_ip_record &b) {
        if (a.match_area != b.match_area) {
            if (a.match_area) {
                return true;
            } else {
                return false;
            }
        } else {
            if (a.forbid != b.forbid) {
                if (b.forbid) {
                    return true;
                } else {
                    return false;
                }
            } else {
                return a.server_order < b.server_order;
            }
        }
    };
};


class dns_record {
public:
    vector<uint32_t> ips;
    uint64_t expire_time = 0;
    string server;
    string domain;
    bool expire = false;
    bool match_area = false;
    uint16_t trusted_ip_count;
    uint16_t total_ip_count;
    string serialize() const;
    static vector<dns_record> transform(const st::dns::proto::records &records);
};


class dns_record_stats {
public:
    uint32_t total_domain = 0;
    uint32_t trusted_domain = 0;
    uint32_t total_ip = 0;
    uint16_t trusted_ip = 0;
    string serialize() const {
        return "total_domain:" + to_string(total_domain) + "\ntrusted_domain:" + to_string(trusted_domain) + "\ntotal_ip:" + to_string(total_ip) + "\ntrusted_ip:" + to_string(trusted_ip);
    }
};
class dns_record_manager {
private:
    st::kv::disk_kv db;

public:
    dns_record_manager();

    static dns_record_manager &uniq();

    std::vector<dns_record> get_dns_record_list(const string &domain);

    st::dns::proto::records get_dns_records_pb(const string &domain);

    void add(const string &domain, const vector<uint32_t> &ips, const string &dns_server, int expire);

    void query(const string &domain, dns_record &record);
    dns_record query(const string &domain);

    void remove(const string &domain);

    void clear();

    dns_record_stats stats();

    bool has_any_record(const string &domain);

    std::string dump();
    static dns_record transform(const st::dns::proto::records &records);
};

#endif//ST_DNS_DNS_RECORD_MANAGER_H
