//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_BYTEUTILS_H
#define ST_BYTEUTILS_H

#include <iostream>
#include <string>
#include <vector>

using namespace std;
typedef uint8_t byte;

namespace st {
    namespace utils {
        inline void copyByte(byte *from, byte *to, int size) {
            for (auto i = 0L; i < size; i++) { *(to + i) = *(from + i); }
        }

        template<typename ItemType, typename Num>
        inline void copy(ItemType *from, ItemType *to, Num len) {
            for (auto i = 0L; i < len; i++) { *(to + i) = *(from + i); }
        };

        template<typename ItemType, typename Num>
        inline void copy(const char *from, ItemType &to, Num len) {
            for (auto i = 0L; i < len; i++) { to[i] = *(from + i); }
        };

        template<typename ItemType, typename ItemTypeB, typename NumA, typename NumB, typename NumC>
        inline void copy(ItemType *from, ItemTypeB *to, NumA indexFrom, NumB distFrom,
                         NumC len) {
            for (auto i = 0L; i < len; i++) { *(to + distFrom + i) = *(from + indexFrom + i); }
        };

        template<typename Num>
        inline void toBytes(byte *byteArr, Num num) {
            uint8_t len = sizeof(Num);
            for (auto i = 0; i < len; i++) {
                uint64_t move = (len - i - 1) * 8U;
                uint64_t mask = 0xFFU << move;
                *(byteArr + i) = (num & mask) >> move;
            }
        };

        template<typename IntTypeB>
        inline void read(const byte *data, IntTypeB &result) {
            uint8_t len = sizeof(IntTypeB);
            for (uint8_t i = 0; i < len; i++) {
                byte val = *(data + i);
                uint32_t bitMove = (len - i - 1) * 8U;
                uint32_t valFinal = val << bitMove;
                result |= valFinal;
            }
        }


        template<typename Collection>
        inline string join(const Collection &lists, const char *delimit) {
            string result;
            for (auto value : lists) {
                if (!result.empty()) { result += delimit; }
                result += value;
            }
            return result;
        }

        template<typename IntTypeB>
        inline byte *write(byte *data, IntTypeB &result) {
            uint8_t len = sizeof(IntTypeB);
            for (uint8_t i = 0; i < len; i++) {
                auto i1 = (len - i - 1) * 8;
                *(data + i) = (((0xFF << i1) & result) >> i1);
            }
            return data + len;
        }
    }// namespace utils
}// namespace st


#endif//ST_BYTEUTILS_H
