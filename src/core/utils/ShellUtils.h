#ifndef SHELL_UTILS_H
#define SHELL_UTILS_H

#include <string>
#include "boost/process.hpp"

using namespace std;
namespace st {
    namespace utils {
        namespace shell {
            static bool exec(const string &command, string &resultStr, string &errorStr) {
                std::error_code ec;
                boost::process::ipstream is;
                boost::process::ipstream error;
                int result = boost::process::system(command, boost::process::std_out > is, boost::process::std_err > error, ec);
                bool success = false;
                if (!ec && result == 0) {
                    string log;
                    while (getline(is, log)) {
                        if (!resultStr.empty()) {
                            resultStr += "\n";
                        }

                        resultStr += log;
                    }
                    success = true;
                }
                string log;
                while (getline(error, log)) {
                    if (!errorStr.empty()) {
                        errorStr += "\n";
                    }
                    errorStr += log;
                }
                is.close();
                error.close();
                return success;
            }

        }
    }
}

#endif