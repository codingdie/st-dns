//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_FILEUTILS_H
#define ST_FILEUTILS_H

#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include "Logger.h"

namespace st {
    namespace utils {
        namespace file {

            static bool pid(const string &pidFile) {
                int pid = getpid();
                ofstream fileStream(pidFile);
                if (fileStream.is_open()) {
                    fileStream << pid;
                    fileStream.flush();
                    fileStream.close();
                    return true;
                }
                return false;
            }

            static bool exit(const string &path) {
                bool exits = false;
                fstream fileStream;
                fileStream.open(path, ios::in);
                if (fileStream) {
                    exits = true;
                }
                fileStream.close();
                return exits;
            }
            static bool del(const string &path) {
                boost::filesystem::path bpath(path);
                boost::filesystem::remove(bpath);
            }

            static bool createIfNotExits(const string &path) {
                bool result = false;
                try {
                    boost::filesystem::path bpath(path);
                    if (!boost::filesystem::exists(bpath.parent_path())) {
                        boost::system::error_code errorCode;
                        boost::filesystem::create_directories(bpath.parent_path(), errorCode);
                        if (errorCode) {
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

            static void read(const string &path, unordered_set<string> &data) {
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
        }// namespace file
    }    // namespace utils
}// namespace st
#endif//ST_FILEUTILS_H
