#include "unzip_name.h"

#include <stdint.h>

static int ascii_upper(int ch)
{
    return ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch;
}

static int reserved_device(const char *name, size_t length)
{
    size_t stem = 0;
    int first, second, third;

    while (stem < length && name[stem] != '.') {
        ++stem;
    }
    if (stem == 3) {
        first = ascii_upper((unsigned char)name[0]);
        second = ascii_upper((unsigned char)name[1]);
        third = ascii_upper((unsigned char)name[2]);
        return (first == 'C' && second == 'O' && third == 'N') ||
               (first == 'P' && second == 'R' && third == 'N') ||
               (first == 'A' && second == 'U' && third == 'X') ||
               (first == 'N' && second == 'U' && third == 'L');
    }
    if (stem == 4) {
        first = ascii_upper((unsigned char)name[0]);
        second = ascii_upper((unsigned char)name[1]);
        third = ascii_upper((unsigned char)name[2]);
        return ((first == 'C' && second == 'O' && third == 'M') ||
                (first == 'L' && second == 'P' && third == 'T')) &&
               name[3] >= '1' && name[3] <= '9';
    }
    if (stem == 6) {
        return ascii_upper((unsigned char)name[0]) == 'C' &&
               ascii_upper((unsigned char)name[1]) == 'O' &&
               ascii_upper((unsigned char)name[2]) == 'N' &&
               ascii_upper((unsigned char)name[3]) == 'I' &&
               ascii_upper((unsigned char)name[4]) == 'N' &&
               name[5] == '$';
    }
    if (stem == 7) {
        return ascii_upper((unsigned char)name[0]) == 'C' &&
               ascii_upper((unsigned char)name[1]) == 'O' &&
               ascii_upper((unsigned char)name[2]) == 'N' &&
               ascii_upper((unsigned char)name[3]) == 'O' &&
               ascii_upper((unsigned char)name[4]) == 'U' &&
               ascii_upper((unsigned char)name[5]) == 'T' &&
               name[6] == '$';
    }
    return 0;
}

static int utf8_valid(const char *name, size_t length)
{
    size_t index = 0;

    while (index < length) {
        uint32_t codepoint;
        unsigned char first = (unsigned char)name[index++];
        unsigned int following;

        if (first < 0x80U) {
            continue;
        }
        if (first >= 0xc2U && first <= 0xdfU) {
            codepoint = first & 0x1fU;
            following = 1;
        } else if (first >= 0xe0U && first <= 0xefU) {
            codepoint = first & 0x0fU;
            following = 2;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            codepoint = first & 0x07U;
            following = 3;
        } else {
            return 0;
        }
        if (following > length - index) {
            return 0;
        }
        while (following-- != 0U) {
            unsigned char next = (unsigned char)name[index++];
            if ((next & 0xc0U) != 0x80U) {
                return 0;
            }
            codepoint = (codepoint << 6U) | (next & 0x3fU);
        }
        if (codepoint < 0x80U ||
            (codepoint < 0x800U && first >= 0xe0U) ||
            (codepoint < 0x10000U && first >= 0xf0U) ||
            codepoint > 0x10ffffU ||
            (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
            return 0;
        }
    }
    return 1;
}

int unzip_name_validate(const char *name, size_t length, int is_directory)
{
    size_t start = 0;
    size_t index;

    if (name == NULL || length == 0 ||
        (is_directory && name[length - 1U] != '/') ||
        (!is_directory && name[length - 1U] == '/') ||
        !utf8_valid(name, length)) {
        return 0;
    }
    for (index = 0; index < length; ++index) {
        unsigned char ch = (unsigned char)name[index];
        size_t segment_length;

        if (ch == 0 || ch < 0x20U || ch == 0x7fU || ch == '\\' ||
            ch == ':' || ch == '<' || ch == '>' || ch == '"' ||
            ch == '|' || ch == '?' || ch == '*') {
            return 0;
        }
        if (ch != '/' && index + 1U != length) {
            continue;
        }
        segment_length = index - start + (ch == '/' ? 0U : 1U);
        if (segment_length == 0 ||
            (segment_length == 1 && name[start] == '.') ||
            (segment_length == 2 && name[start] == '.' &&
             name[start + 1U] == '.') ||
            name[start + segment_length - 1U] == '.' ||
            name[start + segment_length - 1U] == ' ' ||
            reserved_device(name + start, segment_length)) {
            return 0;
        }
        start = index + 1U;
    }
    return 1;
}
