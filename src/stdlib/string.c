#include <stdint.h>
#include <stddef.h>
#include "header/stdlib/string.h"

void* memset(void *s, int c, size_t n) {
    uint8_t *buf = (uint8_t*) s;
    for (size_t i = 0; i < n; i++)
        buf[i] = (uint8_t) c;
    return s;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    uint8_t *dstbuf       = (uint8_t*) dest;
    const uint8_t *srcbuf = (const uint8_t*) src;
    for (size_t i = 0; i < n; i++)
        dstbuf[i] = srcbuf[i];
    return dstbuf;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *buf1 = (const uint8_t*) s1;
    const uint8_t *buf2 = (const uint8_t*) s2;
    for (size_t i = 0; i < n; i++) {
        if (buf1[i] < buf2[i])
            return -1;
        else if (buf1[i] > buf2[i])
            return 1;
    }

    return 0;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *dstbuf       = (uint8_t*) dest;
    const uint8_t *srcbuf = (const uint8_t*) src;
    if (dstbuf < srcbuf) {
        for (size_t i = 0; i < n; i++)
            dstbuf[i]   = srcbuf[i];
    } else {
        for (size_t i = n; i != 0; i--)
            dstbuf[i-1] = srcbuf[i-1];
    }

    return dest;
}

int strlen(const char *str) {
    int length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

void getWord(const char* str, uint16_t wordIdx, char* buf) {
    int counter = 0;
    int idx = -1;
    int n = strlen(str);

    for (int i = 0; i < n; i++) {
        if (i == 0 && str[i] != ' ') {
            counter++;
            if (wordIdx+1 == counter) {
                idx = i;
                break;
            }
        } else if (str[i] == ' ' && str[i+1] != ' ') {
            counter++;
            if (wordIdx+1 == counter) {
                idx = i+1;
                break;
            }
        }
    }

    if (idx == -1) {
        buf[0] = '\0';
        return;
    } else {
        int i = 0;
        while (str[idx] != ' ' && idx < n) {
            buf[i] = str[idx];
            i++;
            idx++;
        }
        buf[i] = '\0';
    }
}

void clear(void *pointer, size_t n) {
    uint8_t *ptr       = (uint8_t*) pointer;
    for (size_t i = 0; i < n; i++) {
        ptr[i] = 0x00;
    }
}

void concat(const char* char1, const char* char2, char* charconcat) {
    int len1 = strlen(char1);
    int len2 = strlen(char2);

    for (int i = 0; i < len1; i++) {
        charconcat[i] = char1[i];
    }
    
    charconcat[len1] = '/';
    
    for (int i = 0; i < len2; i++) {
        charconcat[len1 + 1 + i] = char2[i];
    }

    charconcat[len1 + len2 + 1] = '\0';
}