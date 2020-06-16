#include <iostream>
#include "Config.h"
#include "DNSServer.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

using namespace boost::asio;
typedef boost::shared_ptr<ip::tcp::socket> socket_ptr;

st::dns::Config getConfig(int argc, char *const *argv);

int main(int argc, char *argv[]) {

    try {

        st::dns::Config config = getConfig(argc, argv);
        DNSServer dnsServer(config);
        dnsServer.start();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

st::dns::Config getConfig(int argc, char *const *argv) {
    if (argc > 1) {
        return st::dns::Config(string(argv[1]));
    } else {
        return st::dns::Config("/etc/config/st-dns/config.json");
    }
}