//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_FILEUTILS_H
#define ST_FILEUTILS_H

#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <set>

namespace st {
    namespace utils {
        namespace file {
            static inline bool exit(const string &path) {
                bool exits = false;
                fstream fileStream;
                fileStream.open(path, ios::in);
                if (fileStream) {
                    exits = true;
                }
                fileStream.close();
                return exits;
            }

            static inline bool createIfNotExits(const string &path) {
                bool result = false;
                try {
                    boost::filesystem::path bpath(path);
                    if (!boost::filesystem::exists(bpath.parent_path())) {
                        boost::system::error_code errorCode;
                        boost::filesystem::create_directories(bpath.parent_path(), errorCode);
                        if (errorCode.failed()) {
                            return false;
                        }
                    }
                    if (!exit(path)) {
                        ofstream fout;
                        fout.open(path, ios::app);
                        if (fout.is_open()) {
                            result = true;
                        }
                        fout.close();
                    } else {
                        result = true;
                    }
                } catch (exception ex) {
                    Logger::ERROR << "create file failed!" << ex.what() << path << END;
                }
                return result;


            }

            static void read(const string &path, set <string> &data) {
                fstream fileStream;
                fileStream.open(path, ios::in);
                if (fileStream) {
                    string line;
                    while (getline(fileStream, line)) {
                        data.emplace(line);
                    }
                    fileStream.close();
                }

            }
        }
    }
}
#endif //ST_FILEUTILS_H