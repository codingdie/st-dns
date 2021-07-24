//
// Created by codingdie on 2020/5/24.
//

#include "DNSCache.h"
#include "Config.h"
#include <random>
#include <vector>
DNSCache DNSCache::INSTANCE;

static mutex rLock;

void DNSCache::addCache(const string &domain, const unordered_set<uint32_t> &ips, const string &dnsServer, const int expire,
                        const bool matchArea) {
    {
        lock_guard<mutex> lockGuard(rLock);
        unordered_map<string, DNSRecord> &records = caches[domain];
        DNSRecord &record = records[dnsServer];
        if (!record.matchArea && matchArea) {
            trustedRecordCount++;
        }
        record.ips = ips;
        record.dnsServer = dnsServer;
        record.host = domain;
        record.matchArea = matchArea;
        record.expireTime = time::now() + expire * 1000;
        Logger::INFO << "addDNSCache" << domain << ipv4::ipsToStr(ips) << "from" << dnsServer << "expire" << expire
                     << record.expireTime << END;
        ofstream fs(st::dns::Config::INSTANCE.dnsCacheFile, std::ios_base::out | std::ios_base::app);
        if (fs.is_open()) {
            fs << record.serialize() << "\n";
            fs.flush();
        }
    }
}
bool DNSCache::hasAnyRecord(const string &host) {
    return caches.find(host) != caches.end();
}

void DNSCache::query(const string &host, DNSRecord &record) {
    uint64_t beginTime = time::now();
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    record.host = host;

    auto iterator = caches.find(host);
    if (iterator != caches.end()) {
        unordered_map<string, DNSRecord> &maps = iterator->second;
        for (auto it = maps.begin(); it != maps.end(); it++) {
            record = it->second;
            if (record.expireTime > st::utils::time::now()) {
                record.expire = false;
            } else {
                record.expire = true;
            }
            vector<uint32_t> ips(record.ips.begin(), record.ips.end());
            std::shuffle(ips.begin(), ips.end(), std::default_random_engine(seed));
            record.ips = std::unordered_set<uint32_t>(ips.begin(), ips.end());
            if (record.matchArea) {
                break;
            }
        }
    }
}

unordered_set<string> DNSCache::queryNotMatchAreaServers(const string &host) {
    unordered_set<string> result;
    auto iterator = caches.find(host);
    if (iterator != caches.end()) {
        unordered_map<string, DNSRecord> &maps = iterator->second;
        for (auto it = maps.begin(); it != maps.end(); it++) {
            auto record = it->second;
            if (!record.matchArea) {
                result.emplace(record.dnsServer);
            }
        }
    }
    return move(result);
}

DNSCache::DNSCache() {
}

void DNSCache::loadFromFile() {
    {
        lock_guard<mutex> lockGuard(rLock);
        string path = st::dns::Config::INSTANCE.baseConfDir + "/cache.txt";
        if (!st::dns::Config::INSTANCE.dnsCacheFile.empty()) {
            path = st::dns::Config::INSTANCE.dnsCacheFile;
        }
        ifstream in(path);
        string line;
        int count = 0;
        while (getline(in, line)) {//获取文件的一行字符串到line中
            DNSRecord record;
            if (record.deserialize(line)) {
                caches[record.host][record.dnsServer] = record;
                count++;
                if (record.matchArea) {
                    trustedRecordCount++;
                }
            }
        }
        Logger::INFO << count << "dns record loadFromFile" << path << END;
    }
    saveToFile();
}

void DNSCache::saveToFile() {
    lock_guard<mutex> lockGuard(rLock);
    ofstream fileStream(st::dns::Config::INSTANCE.dnsCacheFile);
    if (fileStream.is_open()) {
        int count = 0;
        for (auto it = caches.begin(); it != caches.end(); it++) {
            auto serverRecords = it->second;
            for (auto it2 = serverRecords.begin(); it2 != serverRecords.end(); it2++) {
                fileStream << it2->second.serialize() << "\n";
                count++;
            }
        }
        fileStream.flush();
        fileStream.close();
        Logger::INFO << count << "dns record saveToFile" << st::dns::Config::INSTANCE.dnsCacheFile << END;
    }
}

uint32_t DNSCache::getTrustedCount() {
    return trustedRecordCount;
}

string DNSRecord::serialize() {
    return dnsServer + "\t" + host + "\t" + to_string(expireTime) + "\t" + to_string(matchArea) + "\t" +
           st::utils::ipv4::ipsToStr(ips);
}
bool DNSRecord::deserialize(const string &str) {
    const vector<std::string> &vector = st::utils::strutils::split(str, "\t");
    if (vector.size() != 5) {
        return false;
    }
    dnsServer = vector[0];
    host = vector[1];
    expireTime = stoull(vector[2]);
    matchArea = stoi(vector[3]);
    ips = ipv4::strToIps(vector[4]);
    return true;
}
boost::property_tree::ptree DNSRecord::toPT() {
    boost::property_tree::ptree pt;
    pt.put<string>("dnsServer", dnsServer);
    pt.put<uint64_t>("expireTime", expireTime);
    pt.put<bool>("matchArea", matchArea);
    pt.put<string>("ips", st::utils::ipv4::ipsToStr(ips));
    return pt;
}