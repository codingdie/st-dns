#ifndef SHELL_UTILS_H
#define SHELL_UTILS_H

#include "Logger.h"
#include "boost/process.hpp"
#include <string>
using namespace std;
namespace st {
    namespace utils {
        namespace shell {
            static bool exec(const string &command, string &resultStr, string &errorStr) {
                resultStr = "";
                std::error_code ec;
                boost::process::ipstream is;
                boost::process::ipstream error;
                int result = boost::process::system(command, boost::process::std_out > is, boost::process::std_err > error, ec);
                bool success = false;
                if (!ec && result == 0) {
                    success = true;
                }
                string log;
                while (getline(is, log)) {
                    if (!resultStr.empty()) {
                        resultStr += "\n";
                    }
                    resultStr += log;
                }
                log = "";
                while (getline(error, log)) {
                    if (!errorStr.empty()) {
                        errorStr += "\n";
                    }
                    errorStr += log;
                }
                return success;
            }
            static bool exec(const string &command, string &resultStr) {
                resultStr = "";
                string error;
                int result = exec(command, resultStr, error);
                if (!error.empty()) {
                    Logger::ERROR << error << END;
                }
                return result;
            }
            static bool exec(const string &command) {
                std::error_code ec;
                int result = boost::process::system(command, ec);
                bool success = false;
                if (!ec && result == 0) {
                    success = true;
                }
                return success;
            }
        }// namespace shell
    }    // namespace utils
}// namespace st

#endif