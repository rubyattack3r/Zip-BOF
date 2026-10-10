#ifndef ZIP_BOFPE_ZIP_ARCHIVE_H
#define ZIP_BOFPE_ZIP_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include "zip_path.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZIP_ERROR_CAPACITY 512
#ifndef ZIP_MEMORY_INPUT_LIMIT
#define ZIP_MEMORY_INPUT_LIMIT (64ULL * 1024ULL * 1024ULL)
#endif
#ifndef ZIP_MEMORY_ENTRY_LIMIT
#define ZIP_MEMORY_ENTRY_LIMIT 100000ULL
#endif

typedef struct zip_result {
    uint64_t files;
    uint64_t directories;
    uint64_t bytes;
    uint64_t skipped_links;
    char error[ZIP_ERROR_CAPACITY];
} zip_result;

/*
 * Archives source_path as a single root entry at destination_path.
 * compression_level is in the inclusive range 0..9.
 * Returns 0 on success and -1 on failure; failure details are in result.
 */
int zip_create_archive(const zip_path_char *source_path,
                       const zip_path_char *destination_path,
                       int compression_level,
                       zip_result *result);

/*
 * Creates the same archive entirely in memory. excluded_output_path is omitted
 * if it is beneath source_path. The caller owns *archive_data on success and
 * must release it with zip_free_archive().
 */
int zip_create_archive_memory(const zip_path_char *source_path,
                              const zip_path_char *excluded_output_path,
                              int compression_level,
                              void **archive_data,
                              size_t *archive_size,
                              zip_result *result);

void zip_free_archive(void *archive_data, size_t archive_size);

#ifdef __cplusplus
}
#endif

#endif
