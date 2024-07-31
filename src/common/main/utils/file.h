//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_FILEUTILS_H
#define ST_FILEUTILS_H

#include "logger.h"
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <unordered_set>

namespace st {
    namespace utils {
        namespace file {

            inline bool pid(const string &pidFile) {
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

            inline bool exists(const string &path) {
                return boost::filesystem::is_regular_file(path);
            }
            inline void del(const string &path) {
                boost::filesystem::path bpath(path);
                boost::filesystem::remove(bpath);
            }
            inline bool mkdirs(const string &path) {
                boost::system::error_code error;
                boost::filesystem::path bpath(path);
                if (boost::filesystem::exists(bpath) && boost::filesystem::is_directory(path)) {
                    return true;
                }
                boost::filesystem::create_directories(bpath, error);
                if (error) {
                    std::cerr << error << std::endl;
                    return false;
                }
            }

            inline bool create_if_not_exits(const string &path) {
                bool result = false;
                try {
                    boost::filesystem::path bpath(path);
                    if (!boost::filesystem::exists(bpath.parent_path())) {
                        boost::system::error_code errorCode;
                        boost::filesystem::create_directories(bpath.parent_path(), errorCode);
                        if (errorCode) {
                            std::cerr << errorCode << std::endl;
                            return false;
                        }
                    }
                    if (!exists(path)) {
                        ofstream fout;
                        fout.open(path, ios::app);
                        if (fout.is_open()) {
                            result = true;
                        }
                        fout.close();
                    } else {
                        result = true;
                    }
                } catch (exception &ex) {
                    logger::ERROR << "create file failed!" << ex.what() << path << END;
                }
                return result;
            }

            inline void read(const string &path, unordered_set<string> &data) {
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
            inline string read(const string &path) {
                fstream fileStream;
                string result;
                fileStream.open(path, ios::in);
                if (fileStream) {
                    string line;
                    while (getline(fileStream, line)) {
                        result += line;
                        result += "\n";
                    }
                    fileStream.close();
                }
                return result;
            }
            inline uint32_t limit_file_cnt(const string &path, const uint32_t limit) {
                vector<pair<std::time_t, boost::filesystem::path>> files;
                for (const auto &it : boost::filesystem::directory_iterator(path)) {
                    if (is_regular(it.path())) {
                        files.emplace_back(last_write_time(it.path()), it.path());
                    }
                }
                std::sort(files.begin(), files.end(), [](pair<std::time_t, boost::filesystem::path> &a, pair<std::time_t, boost::filesystem::path> &b) {
                    return a.first < b.first;
                });
                uint32_t need_delete_cnt = files.size() > limit ? files.size() - limit : 0;
                uint32_t result = need_delete_cnt;
                if (need_delete_cnt > 0) {
                    for (const auto &item : files) {
                        boost::filesystem::remove(item.second);
                        logger::INFO << "delete file" << item.second.string() << item.first << END;
                        need_delete_cnt--;
                        if (need_delete_cnt <= 0) {
                            break;
                        }
                    }
                }

                return result;
            }
            inline uint32_t get_file_cnt(const string &path) {
                uint32_t cnt = 0;
                for (const auto &it : boost::filesystem::directory_iterator(path)) {
                    if (is_regular(it.path())) {
                        cnt++;
                    }
                }
                return cnt;
            }
        }// namespace file
    }// namespace utils
}// namespace st
#endif//ST_FILEUTILS_H
