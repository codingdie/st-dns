//
// Created by codingdie on 2020/5/24.
//

#include "dns_cache.h"
#include "config.h"
#include <random>
#include <vector>
dns_cache dns_cache::INSTANCE;

static mutex rLock;

void dns_cache::add(const string &domain, const vector<uint32_t> &ips, const string &dnsServer, const int expire,
                    const bool match_area) {
    {
        if (!has_trusted_record(domain) && match_area) {
            trusted_domain_count++;
        }
        lock_guard<mutex> lg(rLock);
        unordered_map<string, dns_record> &records = caches[domain];
        dns_record &record = records[dnsServer];

        for (auto ip : ips) {
            st::dns::shm::share().add_reverse_record(ip, domain, match_area);
        }
        record.ips = ips;
        record.server = dnsServer;
        record.domain = domain;
        record.match_area = match_area;
        record.expire_time = time::now() + expire * 1000;
        logger::INFO << "add dns cache" << domain << ipv4::ips_to_str(ips) << "from" << dnsServer << "expire" << expire
                     << "match_area" << match_area << END;
        ofstream fs(st::dns::config::INSTANCE.dns_cache_file, std::ios_base::out | std::ios_base::app);
        if (fs.is_open()) {
            fs << record.serialize() << "\n";
            fs.flush();
        }
    }
}
bool dns_cache::has_trusted_record(const string &domain) {
    if (!has_any_record(domain)) {
        return false;
    }
    lock_guard<mutex> lg(rLock);
    for (auto it = caches[domain].begin(); it != caches[domain].end(); it++) {
        if (it->second.match_area) {
            return true;
        }
    }
    return false;
}

bool dns_cache::has_any_record(const string &domain) {
    lock_guard<mutex> lg(rLock);
    return caches.find(domain) != caches.end();
}
uint32_t dns_cache::server_count(const string &domain) {
    lock_guard<mutex> lg(rLock);
    uint32_t count = 0;
    for (auto it = caches.find(domain); it != caches.end(); it++) {
        count++;
    }
    return count;
}

void dns_cache::query(const string &domain, dns_record &record) {
    lock_guard<mutex> lg(rLock);
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    record.domain = domain;
    auto iterator = caches.find(domain);
    vector<ip_quality_info> ip_records;
    if (iterator != caches.end()) {
        unordered_map<string, dns_record> &maps = iterator->second;
        uint8_t server_order = 0;
        for (auto server : st::dns::config::INSTANCE.servers) {
            auto serverId = server->id();
            server_order++;
            if (maps.find(serverId) != maps.end()) {
                dns_record t_record = maps[serverId];
                std::shuffle(t_record.ips.begin(), t_record.ips.end(), std::default_random_engine(seed));

                for (auto ip : t_record.ips) {
                    ip_quality_info tmp;
                    tmp.ip = ip;
                    tmp.forbid = st::proxy::shm::uniq().is_ip_forbid(ip);
                    tmp.server = t_record.server;
                    tmp.expire = t_record.expire_time < st::utils::time::now();
                    tmp.expire_time = t_record.expire_time;
                    tmp.match_area = t_record.match_area;
                    tmp.server_order = server_order;
                    ip_records.emplace_back(tmp);
                }
            }
        }
    }
    std::sort(ip_records.begin(), ip_records.end(), ip_quality_info::compare);
    for (auto &item : ip_records) {
        if (item.match_area && !item.forbid) {
            record.ips.emplace_back(item.ip);
        }
    }
    if (record.ips.empty()) {
        for (auto &item : ip_records) {
            record.ips.emplace_back(item.ip);
        }
    }
    if (!ip_records.empty()) {
        record.match_area = ip_records[0].match_area;
        record.expire_time = ip_records[0].expire_time;
        record.server = ip_records[0].server;
        record.expire = ip_records[0].expire;
    }
}

unordered_set<string> dns_cache::query_not_match_area_servers(const string &domain) {
    lock_guard<mutex> lg(rLock);
    unordered_set<string> result;
    auto iterator = caches.find(domain);
    if (iterator != caches.end()) {
        unordered_map<string, dns_record> &maps = iterator->second;
        for (auto it = maps.begin(); it != maps.end(); it++) {
            auto record = it->second;
            if (!record.match_area) {
                result.emplace(record.server);
            }
        }
    }
    return result;
}

dns_cache::dns_cache() {
}

void dns_cache::load_from_file() {
    {
        lock_guard<mutex> lg(rLock);
        string path = st::dns::config::INSTANCE.base_conf_dir + "/cache.txt";
        if (!st::dns::config::INSTANCE.dns_cache_file.empty()) {
            path = st::dns::config::INSTANCE.dns_cache_file;
        }
        ifstream in(path);
        string line;
        int count = 0;
        while (getline(in, line)) {//获取文件的一行字符串到line中
            dns_record record;
            if (record.deserialize(line)) {
                caches[record.domain][record.server] = record;
                count++;
                for (auto ip : record.ips) {
                    st::dns::shm::share().add_reverse_record(ip, record.domain, record.match_area);
                }
            }
        }
        logger::INFO << count << "dns record load_from_file" << path << END;
    }
    saveToFile();
}

void dns_cache::saveToFile() {
    lock_guard<mutex> lg(rLock);
    ofstream fs(st::dns::config::INSTANCE.dns_cache_file);
    if (fs.is_open()) {
        int count = 0;
        for (auto it = caches.begin(); it != caches.end(); it++) {
            auto server_records = it->second;
            for (auto it2 = server_records.begin(); it2 != server_records.end(); it2++) {
                fs << it2->second.serialize() << "\n";
                count++;
                if (it2->second.match_area) {
                    trusted_domain_count++;
                }
            }
        }
        fs.flush();
        fs.close();
        logger::INFO << count << "dns record saveToFile" << st::dns::config::INSTANCE.dns_cache_file << END;
    }
}

uint32_t dns_cache::get_trusted_domain_count() {
    return trusted_domain_count;
}

string dns_record::serialize() {
    return server + "\t" + domain + "\t" + to_string(expire_time) + "\t" + to_string(match_area) + "\t" +
           st::utils::ipv4::ips_to_str(ips);
}
bool dns_record::deserialize(const string &str) {
    const vector<std::string> &vector = st::utils::strutils::split(str, "\t");
    if (vector.size() != 5) {
        return false;
    }
    server = vector[0];
    domain = vector[1];
    expire_time = stoull(vector[2]);
    match_area = stoi(vector[3]);
    ips = ipv4::str_to_ips(vector[4]);
    return true;
}