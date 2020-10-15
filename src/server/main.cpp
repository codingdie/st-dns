#include <iostream>
#include "Config.h"
#include "DNSServer.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

using namespace boost::asio;

int main(int argc, char *argv[]) {
    if (argc > 1) {
        st::dns::Config::INSTANCE.load(string(argv[1]));
    } else {
        st::dns::Config::INSTANCE.load("/etc/config/st-dns/");
    }
    DNSServer dnsServer(st::dns::Config::INSTANCE);
    dnsServer.start();
    return 0;
}

