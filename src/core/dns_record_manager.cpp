//
// Created by codingdie on 2020/5/24.
//

#include "dns_record_manager.h"
#include "config.h"
#include <random>
#include <vector>
#include "protocol/message.pb.h"

void dns_record_manager::add(const string &domain, const vector<uint32_t> &ips, const string &dns_server, const int expire) {
    st::dns::proto::records dns_records = get_dns_records_pb(domain);
    dns_records.set_domain(domain);
    st::dns::proto::record pb;
    pb.set_expire(expire + time::now() / 1000);
    for (const auto &ip : ips) {
        pb.add_ips(ip);
        st::dns::shm::share().add_reverse_record(ip, domain, true);
    }
    (*dns_records.mutable_map())[dns_server] = pb;
    db.put(domain, dns_records.SerializeAsString());
    logger::INFO << "add dns cache" << domain << ipv4::ips_to_str(ips) << "from" << dns_server << "expire" << expire << END;
}

bool dns_record_manager::has_any_record(const string &domain) {
    return get_dns_records_pb(domain).map_size() > 0;
}

dns_record dns_record_manager::query(const string &domain) {
    auto records = this->get_dns_records_pb(domain);
    return transform(records);
}
dns_record dns_record_manager::transform(const st::dns::proto::records &records) const {
    dns_record record;
    record.domain = records.domain();
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    vector<dns_ip_record> ip_records;
    uint8_t server_order = 0;
    for (auto server : st::dns::config::INSTANCE.servers) {
        auto serverId = server->id();
        server_order++;
        if (records.map().contains(serverId)) {
            st::dns::proto::record t_record = records.map().at(serverId);
            vector<uint32_t> ips(t_record.ips().begin(), t_record.ips().end());
            shuffle(ips.begin(), ips.end(), default_random_engine(seed));
            for (auto ip : ips) {
                dns_ip_record tmp;
                tmp.ip = ip;
                tmp.forbid = proxy::shm::uniq().is_ip_forbid(ip);
                tmp.server = serverId;
                tmp.expire = t_record.expire() * 1000 < time::now();
                tmp.match_area = areaip::manager::uniq().is_area_ip(server->areas, ip);
                tmp.expire_time = t_record.expire();
                tmp.server_order = server_order;
                ip_records.emplace_back(tmp);
            }
        }
    }
    sort(ip_records.begin(), ip_records.end(), dns_ip_record::compare);
    for (auto &item : ip_records) {
        if (item.match_area && !item.forbid) {
            record.ips.emplace_back(item.ip);
            record.trusted_ip_count++;
        }
        record.total_ip_count++;
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
    return record;
}

void dns_record_manager::query(const string &domain, dns_record &record) {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    record.domain = domain;
    auto records = this->get_dns_records_pb(domain);
    vector<dns_ip_record> ip_records;
    uint8_t server_order = 0;
    for (auto server : st::dns::config::INSTANCE.servers) {
        auto serverId = server->id();
        server_order++;
        if (records.map().contains(serverId)) {
            st::dns::proto::record t_record = records.map().at(serverId);
            vector<uint32_t> ips(t_record.ips().begin(), t_record.ips().end());
            std::shuffle(ips.begin(), ips.end(), std::default_random_engine(seed));
            for (auto ip : ips) {
                dns_ip_record tmp;
                tmp.ip = ip;
                tmp.forbid = st::proxy::shm::uniq().is_ip_forbid(ip);
                tmp.server = serverId;
                tmp.expire = t_record.expire() * 1000 < st::utils::time::now();
                tmp.match_area = areaip::manager::uniq().is_area_ip(server->areas, ip);
                tmp.expire_time = t_record.expire();
                tmp.server_order = server_order;
                ip_records.emplace_back(tmp);
            }
        }
    }
    std::sort(ip_records.begin(), ip_records.end(), dns_ip_record::compare);
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


dns_record_manager::dns_record_manager() : db("st-dns-record", 1024 * 1024) {
}


st::dns::proto::records dns_record_manager::get_dns_records_pb(const string &domain) {
    string data = db.get(domain);
    st::dns::proto::records record;
    if (!data.empty()) {
        record.ParseFromString(data);
    }
    return record;
}
dns_record_manager &dns_record_manager::uniq() {
    static dns_record_manager INSTANCE;
    return INSTANCE;
}
std::vector<dns_record> dns_record_manager::get_dns_record_list(const string &domain) {
    st::dns::proto::records records = get_dns_records_pb(domain);
    return dns_record::transform(records);
}
vector<dns_record> dns_record::transform(const st::dns::proto::records &records) {
    vector<dns_record> result;
    if (!records.domain().empty()) {
        for (const auto &server : st::dns::config::INSTANCE.servers) {
            const string &serverId = server->id();
            if (records.map().contains(serverId)) {
                auto item = records.map().at(serverId);
                dns_record record;
                record.ips = vector<uint32_t>(item.ips().begin(), item.ips().end());
                record.expire_time = item.expire();
                record.server = serverId;
                record.domain = records.domain();
                for (const auto &ip : record.ips) {
                    if (areaip::manager::uniq().is_area_ip(server->areas, ip)) {
                        record.match_area = true;
                        break;
                    }
                }
                result.emplace_back(record);
            }
        }
    }
    return result;
}
std::string dns_record_manager::dump() {
    auto path = "/tmp/st-dns-record.txt";
    ofstream fs(path, std::ios_base::out | std::ios_base::trunc);
    if (fs.is_open()) {
        db.list([&fs](const std::string &key, const std::string &value) {
            st::dns::proto::records records;
            records.ParseFromString(value);
            if (!records.domain().empty()) {
                for (const auto &record : dns_record::transform(records)) {
                    fs << record.serialize() << "\n";
                }
                fs.flush();
            }
        });
    }
    return path;
}
void dns_record_manager::remove(const string &domain) {
    db.erase(domain);
}


void dns_record_manager::clear() {
    db.clear();
}
dns_record_stats dns_record_manager::stats() {
    dns_record_stats stats;
    db.list([this, &stats](const std::string &key, const std::string &value) {
        st::dns::proto::records records;
        records.ParseFromString(value);
        if (!records.domain().empty()) {
            stats.total_domain++;
            auto record = transform(records);
            stats.total_ip += record.total_ip_count;
            stats.trusted_ip += record.trusted_ip_count;
            if (record.match_area) {
                stats.trusted_domain++;
            }
        }
    });
    return stats;
}

string dns_record::serialize() const {
    return server + "\t" + domain + "\t" + to_string(expire_time) + "\t" + to_string(match_area) + "\t" +
           st::utils::ipv4::ips_to_str(ips);
}