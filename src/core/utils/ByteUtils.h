//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_BYTEUTILS_H
#define ST_BYTEUTILS_H

#include <iostream>

typedef uint8_t byte;

static void copyByte(byte *from, byte *to, int size) {
    for (int i = 0; i < size; i++) {
        *(to + i) = *(from + i);
    }
}

#endif //ST_BYTEUTILS_H
