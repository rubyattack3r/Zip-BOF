#include "beacon.h"
#include "unzip_archive.h"

#include <string.h>
#include <wchar.h>

static int valid_packed_wstring(const WCHAR *value, int byte_length)
{
    int index;
    int count;

    if (value == NULL || byte_length < (int)sizeof(WCHAR) ||
        byte_length % (int)sizeof(WCHAR) != 0) {
        return 0;
    }
    count = byte_length / (int)sizeof(WCHAR);
    if (value[count - 1] != L'\0') {
        return 0;
    }
    for (index = 0; index + 1 < count; ++index) {
        if (value[index] == L'\0') {
            return 0;
        }
    }
    return 1;
}

void go(const char *data, int length)
{
    datap parser;
    WCHAR *archive;
    WCHAR *destination;
    int archive_length = 0;
    int destination_length = 0;
    unzip_result result;

    BeaconDataParse(&parser, (char *)data, length);
    archive = (WCHAR *)BeaconDataExtract(&parser, &archive_length);
    destination = (WCHAR *)BeaconDataExtract(&parser, &destination_length);
    if (!valid_packed_wstring(archive, archive_length) ||
        !valid_packed_wstring(destination, destination_length) ||
        parser.length != 0) {
        BeaconPrintf(CALLBACK_ERROR_UTF8,
                     "unzip: invalid UTF-16 path arguments\n");
        return;
    }
    if (unzip_extract_archive(archive, destination, &result) != 0) {
        BeaconPrintf(CALLBACK_ERROR_UTF8, "unzip: %s\n", result.error);
        return;
    }
    BeaconPrintf(CALLBACK_OUTPUT_UTF8,
                 "unzip: extracted %llu file(s), %llu directories, "
                 "%llu bytes\n", (unsigned long long)result.files,
                 (unsigned long long)result.directories,
                 (unsigned long long)result.bytes);
}
