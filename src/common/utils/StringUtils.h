//
// Created by System Administrator on 2020/11/1.
//

#ifndef ST_UTILS_STRINGUTILS_H
#define ST_UTILS_STRINGUTILS_H

#include <string>
#include <vector>

namespace st {
    namespace utils {
        namespace strutils {
            static std::vector<std::string> split(const std::string &str,
                                                  const std::string &split) {
                std::vector<std::string> result;
                if (!str.empty()) {
                    uint64_t pos = 0;
                    uint64_t lastPos = 0;
                    while ((pos = str.find(split, pos)) != str.npos) {
                        std::string line = str.substr(lastPos, (pos - lastPos));
                        pos += split.length();
                        lastPos = pos;
                        result.emplace_back(line);
                    }

                    std::string line = str.substr(lastPos, (str.length() - lastPos));
                    result.emplace_back(line);
                }

                return result;
            }
        }// namespace strutils
    }    // namespace utils
}// namespace st

#endif
