//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_CONFIG_H
#define ST_DNS_CONFIG_H

#include <string>
#include <boost/property_tree/json_parser.hpp>
#include "STUtils.h"

using namespace std;
using namespace boost::property_tree;

using namespace std;
namespace st {
    namespace dns {
        class Config {
        public:
            Config() = default;

            Config(string filename) {
                ptree tree;
                try {
                    read_json(filename, tree);
                } catch (json_parser_error e) {
                    Logger::ERROR << " parse config file " + filename + " error!" << e.message() << END;
                    exit(1);
                }
                this->ip = tree.get("ip", string("127.0.0.1"));
                this->port = stoi(tree.get("port", string("1080")));
            }

            string ip = "127.0.0.1";
            int port = 53;
            string dnsServer = "8.8.8.8";
        };

    }
}


#endif //ST_DNS_CONFIG_H
