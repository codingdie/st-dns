//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_DNS_FILEUTILS_H
#define ST_DNS_FILEUTILS_H

#include <iostream>
#include <fstream>

namespace st {
    namespace utils {
        namespace file {
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

            static bool createIfNotExits(const string &path) {
                if (!exit(path)) {
                    ofstream fout;
                    fout.open(path, ios::app);
                    fout.close();
                }

            }
        }
    }
}
#endif //ST_DNS_FILEUTILS_H
