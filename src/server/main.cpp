#include <iostream>
#include "Config.h"
#include "DNSServer.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

using namespace boost::asio;
st::dns::Config getConfig(int argc, char *const *argv);

int main(int argc, char *argv[]) {

    st::dns::Config config = getConfig(argc, argv);
    DNSServer dnsServer(config);
    dnsServer.start();
    return 0;
}

st::dns::Config getConfig(int argc, char *const *argv) {
    if (argc > 1) {
        return st::dns::Config(string(argv[1]));
    } else {
        return st::dns::Config("/etc/config/st-dns/");
    }
}
