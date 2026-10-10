#ifndef ZIP_BOFPE_ZIP_PATH_H
#define ZIP_BOFPE_ZIP_PATH_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <wchar.h>
typedef wchar_t zip_path_char;
#define ZIP_PATH_CAPACITY 32768U
#else
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
typedef char zip_path_char;
#define ZIP_PATH_CAPACITY ((size_t)PATH_MAX)
#endif

enum zip_path_kind {
    ZIP_PATH_MISSING = 0,
    ZIP_PATH_FILE,
    ZIP_PATH_DIRECTORY,
    ZIP_PATH_LINK,
    ZIP_PATH_OTHER
};

#ifdef __cplusplus
extern "C" {
#endif

size_t zip_path_length(const zip_path_char *path);
int zip_path_resolve(const zip_path_char *path, zip_path_char *output,
                     size_t capacity);
int zip_paths_equal(const zip_path_char *left, const zip_path_char *right);
enum zip_path_kind zip_path_get_kind(const zip_path_char *path);
int zip_path_get_file_size(const zip_path_char *path, uint64_t *size);
zip_path_char *zip_path_join(const zip_path_char *parent,
                             const zip_path_char *name,
                             zip_path_char separator);
const zip_path_char *zip_path_basename(const zip_path_char *path);
char *zip_path_to_utf8(const zip_path_char *path, int for_display);

#ifdef __cplusplus
}
#endif

#endif
