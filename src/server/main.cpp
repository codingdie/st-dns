#include <iostream>
#include "Config.h"
#include "DNSServer.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "STUtils.h"

using namespace boost::asio;

int main(int argc, char *argv[]) {
    bool inputConfigPath = false;
    string confPath = "";
    if (argc >= 3 && string(argv[1]) == "-c") {
        confPath = argv[2];
        inputConfigPath = true;
    } else {
        vector<string> availablePaths({"/usr/local/etc/st/dns", "/etc/st/dns"});
        for (auto path:availablePaths) {
            if (st::utils::file::exit(path)) {
                confPath = path;
            }
        }
    }
    if (confPath.empty()) {
        Logger::ERROR << "the config folder not exits!" << END;
        return 0;
    }

    bool startServer = false;
    if (argc == 1) {
        startServer = true;
    } else if (argc == 3 && string(argv[1]) == "-c") {
        startServer = true;
    }
    if (startServer) {
        st::dns::Config::INSTANCE.load(confPath);
        DNSServer dnsServer(st::dns::Config::INSTANCE);
        dnsServer.start();
    } else {
        string serviceOP = "";
        if (inputConfigPath && argc == 5 && string(argv[3]) == "service") {
            serviceOP = string(argv[4]);
        } else if (!inputConfigPath && argc == 3 && string(argv[1]) == "service") {
            serviceOP = string(argv[2]);
        }
        if (!serviceOP.empty()) {
            string error;
            string result;
            bool success = false;
            if (serviceOP == "start") {
                success = shell::exec("sh " + confPath + "/service/start.sh", result, error);
            } else if (serviceOP == "stop") {
                success = shell::exec("sh " + confPath + "/service/stop.sh", result, error);
            }
            if (success) {
                Logger::INFO << result << END;
            } else {
                Logger::ERROR << error << END;
            }
            return 0;
        }

    }
    Logger::ERROR << "not valid command" << END;
    return 1;
}

