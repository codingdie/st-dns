#include <iostream>
#include "Config.h"
#include "DNSServer.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

using namespace boost::asio;
typedef boost::shared_ptr<ip::tcp::socket> socket_ptr;

Config getConfig(int argc, char *const *argv);

int main(int argc, char *argv[]) {
    Config config = getConfig(argc, argv);
    DNSServer dnsServer(config);
    dnsServer.start();
    return 0;
}

Config getConfig(int argc, char *const *argv) {
    if (argc > 1) {
        return Config(string(argv[1]));
    } else {
        return Config("/etc/config/st-dns/config.json");
    }
}
