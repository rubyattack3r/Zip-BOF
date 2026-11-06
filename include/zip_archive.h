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

typedef enum zip_output_mode {
    ZIP_OUTPUT_DOWNLOAD = 0,
    ZIP_OUTPUT_DISK = 1,
    ZIP_OUTPUT_FORCE_DOWNLOAD = 2
} zip_output_mode;

typedef struct zip_options {
    const zip_path_char *source;
    const zip_path_char *destination;
    int compression_level;
    zip_output_mode output_mode;
} zip_options;

/*
 * Archives options->source as a single root entry at options->destination.
 * options->compression_level is in the inclusive range 0..9 and the output
 * mode must be ZIP_OUTPUT_DISK.
 * Returns 0 on success and -1 on failure; failure details are in result.
 */
int zip_create_archive(const zip_options *options,
                       zip_result *result);

/*
 * Creates the same archive entirely in memory. options->destination is omitted
 * if it is beneath options->source. ZIP_OUTPUT_FORCE_DOWNLOAD bypasses the
 * input and entry guards. The caller owns *archive_data on success and must
 * release it with zip_free_archive().
 */
int zip_create_archive_memory(const zip_options *options,
                              void **archive_data,
                              size_t *archive_size,
                              zip_result *result);

void zip_free_archive(void *archive_data, size_t archive_size);

#ifdef __cplusplus
}
#endif

#endif
