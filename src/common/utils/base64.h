
#ifndef ST_UTILS_BASE64_H
#define ST_UTILS_BASE64_H

#include <string>
#include <sstream>
#include <iostream>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace st {
    namespace utils {
        namespace base64 {
             inline std::string encode(const std::string& str) {
                typedef boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8>> base64Encoder;
                std::stringstream result;
                copy(base64Encoder(str.begin()),
                     base64Encoder(str.end()),
                     std::ostream_iterator<char>(result));
                size_t num = (3 - str.length() % 3) % 3;
                for (size_t i = 0; i < num; i++) {
                    result.put('=');
                }
                return result.str();
            }
            inline std::string decode(std::string str) {
                str = boost::replace_all_copy(str, "=", "");
                typedef boost::archive::iterators::transform_width<boost::archive::iterators::binary_from_base64<std::string::const_iterator>, 8, 6>
                        base64_decoder;
                std::stringstream result;
                copy(base64_decoder(str.begin()), base64_decoder(str.end()), std::ostream_iterator<char>(result));
                return result.str();
            }
        }// namespace base64
    }    // namespace utils
}// namespace st

#endif
