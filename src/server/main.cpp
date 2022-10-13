#include "st.h"
#include "config.h"
#include "dns_server.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <iostream>

using namespace boost::asio;
static const vector<string> availablePaths({"../confs", "/usr/local/etc/st/dns", "/etc/st/dns"});
static const string pidFile = "/var/run/st-dns.pid";

void startServer(const string &confPath) {
    st::dns::config::INSTANCE.load(confPath);
    dns_server dnsServer(st::dns::config::INSTANCE);
    dnsServer.start();
}

void serviceScript(const string confPath, const string op) {
    shell::exec("sh " + confPath + "/service/" + op + ".sh");
}

int main(int argc, char *argv[]) {
    bool inputConfigPath = false;
    string confPath;
    if (argc >= 3 && string(argv[1]) == "-c") {
        confPath = argv[2];
        inputConfigPath = true;
    } else {
        for (auto path : availablePaths) {
            if (st::utils::file::exit(path + "/config.json")) {
                confPath = path;
                break;
            }
        }
    }
    if (confPath.empty()) {
        logger::ERROR << "the config folder not exits!" << END;
        return 0;
    }

    bool directStartServer = false;
    if (argc == 1) {
        directStartServer = true;
    } else if (argc == 3 && string(argv[1]) == "-c") {
        directStartServer = true;
    }
    if (directStartServer) {
        file::pid(pidFile);
        startServer(confPath);
    } else {
        string serviceOP = "";
        if (inputConfigPath && argc == 5 && string(argv[3]) == "-d") {
            serviceOP = string(argv[4]);
        } else if (!inputConfigPath && argc == 3 && string(argv[1]) == "-d") {
            serviceOP = string(argv[2]);
        }
        if (!serviceOP.empty()) {
            if (serviceOP == "start" || serviceOP == "stop") {
                serviceScript(confPath, serviceOP);
            } else if (serviceOP == "restart") {
                serviceScript(confPath, "stop");
                serviceScript(confPath, "start");
            } else {
                logger::ERROR << "not support command" << END;
            }
            return 0;
        }
    }
    logger::ERROR << "not valid command" << END;
    return 0;
}
