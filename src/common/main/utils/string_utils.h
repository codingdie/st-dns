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
            inline std::vector<std::string> split(const std::string &str,
                                                  const std::string &separator, const uint32_t &start, const uint32_t &spit_cnt) {
                std::vector<std::string> result;
                if (!str.empty()) {
                    uint64_t pos = start;
                    uint64_t lastPos = 0;
                    uint64_t s_cnt = 0;
                    while ((pos = str.find(separator, pos)) != str.npos) {
                        std::string line = str.substr(lastPos, (pos - lastPos));
                        pos += separator.length();
                        lastPos = pos;
                        result.emplace_back(line);
                        s_cnt++;
                        if (spit_cnt != 0 && s_cnt >= spit_cnt) {
                            break;
                        }
                    }

                    std::string line = str.substr(lastPos, (str.length() - lastPos));
                    result.emplace_back(line);
                }

                return result;
            }
            inline std::vector<std::string> split(const std::string &str,
                                                  const std::string &separator) {
                return split(str, separator, 0, 0);
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
