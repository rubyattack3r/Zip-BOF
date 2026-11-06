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
    zip_options options;
    void *archive_data = NULL;
    size_t archive_size = 0;
    FILE *output;
    size_t written;
    int close_status;

    if (argc < 3 || argc > 5) {
        fprintf(stderr,
                "usage: %s <source> <destination.zip> [level 0-9] "
                "[--disk|--force]\n",
                argv[0]);
        return 2;
    }
    options.source = argv[1];
    options.destination = argv[2];
    options.compression_level = 6;
    options.output_mode = ZIP_OUTPUT_DOWNLOAD;
    if (argc == 4) {
        if (strcmp(argv[3], "--disk") == 0) {
            options.output_mode = ZIP_OUTPUT_DISK;
        } else if (strcmp(argv[3], "--force") == 0) {
            options.output_mode = ZIP_OUTPUT_FORCE_DOWNLOAD;
        } else if (parse_level(argv[3], &options.compression_level) != 0) {
            fprintf(stderr,
                    "error: expected compression level 0-9, --disk, or --force\n");
            return 2;
        }
    }
    if (argc == 5) {
        if (parse_level(argv[3], &options.compression_level) != 0) {
            fprintf(stderr,
                    "error: compression level must be between 0 and 9\n");
            return 2;
        }
        if (strcmp(argv[4], "--disk") == 0) {
            options.output_mode = ZIP_OUTPUT_DISK;
        } else if (strcmp(argv[4], "--force") == 0) {
            options.output_mode = ZIP_OUTPUT_FORCE_DOWNLOAD;
        } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[4]);
            return 2;
        }
    }

    if (options.output_mode == ZIP_OUTPUT_DISK) {
        if (zip_create_archive(&options, &result) != 0) {
            fprintf(stderr, "error: %s\n", result.error);
            return 1;
        }
        goto success;
    }

    if (zip_create_archive_memory(&options, &archive_data,
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
