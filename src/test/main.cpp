//
// Created by codingdie on 2020/5/20.
//

#include <iostream>
#include "DNSClient.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include "DNS.h"

#include <thread>
#include <vector>
#include <chrono>

void testParallel();

using namespace std;
using namespace std::chrono;
using namespace boost::algorithm;
using namespace boost::asio;

long now() {
    auto time_now = std::chrono::system_clock::now();
    auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
    return duration_in_ms.count();
}

int main(int argc, char *argv[]) {
    testParallel();
}

void testParallel() {
    vector<thread *> threads;
    long begin = now();
    int total = 1;
    for (int i = 0; i < total; i++) {
        void (*func)() = []() {
            string domain = "baidu.com";
            string server = "192.168.31.1";
            const string &basicString = DNSClient::udpDns(domain, server);
        };
        thread *thread = new class thread(func);
        threads.push_back(thread);
    }
    int i = 0;
    for (auto it = threads.begin(); it != threads.end(); it++) {
        thread *pThread = *it.base();
        pThread->join();
        delete pThread;
    }
    long end = now();
    long d = (end - begin);
    cout << d << "\t" << d * 1.0 / total << endl;
}
