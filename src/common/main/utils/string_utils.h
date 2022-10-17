//
// Created by System Administrator on 2020/11/1.
//

#ifndef ST_UTILS_STRINGUTILS_H
#define ST_UTILS_STRINGUTILS_H

#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

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
            inline void trim(std::string &str) {
                boost::trim(str);
            }
            inline std::string trim(std::string &&str) {
                std::string result = std::move(str);
                boost::trim(result);
                return result;
            }
            template<typename Collection>
            inline std::string join(const Collection &lists, const char *delimit) {
                std::string result;
                for (auto value : lists) {
                    if (!result.empty()) { result += delimit; }
                    result += value;
                }
                return result;
            }
        }// namespace strutils
    }    // namespace utils
}// namespace st

#endif
