#ifndef UNZIP_ARCHIVE_H
#define UNZIP_ARCHIVE_H

#include "zip_path.h"

#include <stdint.h>

typedef struct unzip_result {
    uint64_t files;
    uint64_t directories;
    uint64_t bytes;
    char error[256];
} unzip_result;

int unzip_extract_archive(const zip_path_char *archive_path,
                          const zip_path_char *destination_path,
                          unzip_result *result);

#endif
