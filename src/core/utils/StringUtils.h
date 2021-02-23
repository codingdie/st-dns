//
// Created by System Administrator on 2020/11/1.
//

#ifndef ST_UTILS_STRINGUTILS_H
#define ST_UTILS_STRINGUTILS_H

#include <string>
#include <vector>
namespace st {
    namespace utils {
        namespace str {
            static inline std::vector<std::string> split(const std::string &str, const std::string &split) {
                std::vector<std::string> result;
                int pos = 0;
                int lastPos = 0;
                while ((pos = str.find(split, pos)) != str.npos) {
                    auto line = str.substr(lastPos, (pos - lastPos));
                    pos += split.length();
                    lastPos = pos;
                    result.emplace_back(line);
                }

                auto line = str.substr(lastPos, (str.length() - lastPos));
                result.emplace_back(line);
                return result;
            }
        }// namespace str
    }    // namespace utils
}// namespace st

#endif
