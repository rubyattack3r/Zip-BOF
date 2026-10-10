#include "zip_archive.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_level(const char *text, int *level)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 || value > 9) {
        return -1;
    }
    *level = (int)value;
    return 0;
}

int main(int argc, char **argv)
{
    zip_result result;
    int level = 6;
    void *archive_data = NULL;
    size_t archive_size = 0;
    FILE *output;
    size_t written;
    int close_status;
    int disk_mode = 0;

    if (argc < 3 || argc > 5) {
        fprintf(stderr,
                "usage: %s <source> <destination.zip> [level 0-9] [--disk]\n",
                argv[0]);
        return 2;
    }
    if (argc == 4) {
        if (strcmp(argv[3], "--disk") == 0) {
            disk_mode = 1;
        } else if (parse_level(argv[3], &level) != 0) {
            fprintf(stderr,
                    "error: expected compression level 0-9 or --disk\n");
            return 2;
        }
    }
    if (argc == 5) {
        if (parse_level(argv[3], &level) != 0) {
            fprintf(stderr,
                    "error: compression level must be between 0 and 9\n");
            return 2;
        }
        if (strcmp(argv[4], "--disk") != 0) {
            fprintf(stderr, "error: unknown option: %s\n", argv[4]);
            return 2;
        }
        disk_mode = 1;
    }

    if (disk_mode) {
        if (zip_create_archive(argv[1], argv[2], level, &result) != 0) {
            fprintf(stderr, "error: %s\n", result.error);
            return 1;
        }
        goto success;
    }

    if (zip_create_archive_memory(argv[1], argv[2], level, &archive_data,
                                  &archive_size, &result) != 0) {
        fprintf(stderr, "error: %s\n", result.error);
        return 1;
    }
    output = fopen(argv[2], "wb");
    if (output == NULL) {
        zip_free_archive(archive_data, archive_size);
        fprintf(stderr, "error: cannot write %s\n", argv[2]);
        return 1;
    }
    written = fwrite(archive_data, 1U, archive_size, output);
    close_status = fclose(output);
    if (written != archive_size || close_status != 0) {
        (void)remove(argv[2]);
        zip_free_archive(archive_data, archive_size);
        fprintf(stderr, "error: cannot write %s\n", argv[2]);
        return 1;
    }
    zip_free_archive(archive_data, archive_size);

success:
    printf("created %s: %llu file(s), %llu directories, %llu input bytes",
           argv[2], (unsigned long long)result.files,
           (unsigned long long)result.directories,
           (unsigned long long)result.bytes);
    if (result.skipped_links != 0) {
        printf(", %llu link(s) skipped",
               (unsigned long long)result.skipped_links);
    }
    putchar('\n');
    return 0;
}
