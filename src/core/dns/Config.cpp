//
// Created by 徐芃 on 2020/5/19.
//

#include "Config.h"
#include <fstream>
#include <iostream>
#include "LogUtils.h"
#include <boost/property_tree/json_parser.hpp>

using namespace std;
using namespace boost::property_tree;

Config::Config(string filename) {
    ptree tree;
    try {
        read_json(filename, tree);
    } catch (json_parser_error e) {
        auto info = " parse config file " + filename + " error! " + e.message();
        LogUtils::error(info);
        exit(1);
    }
    this->ip = tree.get("ip", string("127.0.0.1"));
    this->port = stoi(tree.get("port", string("1080")));
}

Config::Config() {

}
