#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif

#include "zip_path.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
_Static_assert(sizeof(zip_path_char) == 2U,
               "Windows paths require 16-bit wchar_t");
#else
#include <stdio.h>
#include <unistd.h>
#endif

size_t zip_path_length(const zip_path_char *path)
{
    if (path == NULL) {
        return 0U;
    }
#ifdef _WIN32
    return wcslen(path);
#else
    return strlen(path);
#endif
}

#ifndef _WIN32
static int copy_path(zip_path_char *destination, size_t capacity,
                     const zip_path_char *source)
{
    size_t length;

    if (destination == NULL || source == NULL || capacity == 0U) {
        return -1;
    }
    length = zip_path_length(source);
    if (length >= capacity) {
        return -1;
    }
    memcpy(destination, source, (length + 1U) * sizeof(zip_path_char));
    return 0;
}
#endif

#ifdef _WIN32
static size_t windows_root_length(const WCHAR *path)
{
    const WCHAR *cursor;

    if (((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && path[2] == L'\\') {
        return 3U;
    }
    if (path[0] != L'\\' || path[1] != L'\\') {
        return 0U;
    }
    cursor = path + 2;
    while (*cursor != L'\0' && *cursor != L'\\') {
        ++cursor;
    }
    if (*cursor == L'\0') {
        return (size_t)(cursor - path);
    }
    ++cursor;
    while (*cursor != L'\0' && *cursor != L'\\') {
        ++cursor;
    }
    if (*cursor == L'\\') {
        ++cursor;
    }
    return (size_t)(cursor - path);
}
#endif

int zip_path_resolve(const zip_path_char *path, zip_path_char *output,
                     size_t capacity)
{
#ifdef _WIN32
    WCHAR *resolved = NULL;
    const WCHAR extended_drive_prefix[] = L"\\\\?\\";
    const WCHAR extended_unc_prefix[] = L"\\\\?\\UNC\\";
    const WCHAR *suffix;
    size_t prefix_length;
    size_t suffix_length;
    DWORD length;
    size_t index;
    size_t root_length;
    int status = -1;

    if (path == NULL || path[0] == L'\0' || output == NULL ||
        capacity == 0U || capacity > (size_t)0xffffffffU ||
        capacity > SIZE_MAX / sizeof(WCHAR)) {
        return -1;
    }

    /* Device namespaces bypass normal Win32 path parsing. Keep them internal. */
    if ((path[0] == L'\\' || path[0] == L'/') &&
        (path[1] == L'\\' || path[1] == L'/') &&
        (path[2] == L'?' || path[2] == L'.') &&
        (path[3] == L'\\' || path[3] == L'/')) {
        return -1;
    }
    /* Reject drive-relative and current-drive-root paths as ambiguous. */
    if (((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && path[2] != L'\\' && path[2] != L'/') {
        return -1;
    }
    if ((path[0] == L'\\' || path[0] == L'/') &&
        path[1] != L'\\' && path[1] != L'/') {
        return -1;
    }

    resolved = (WCHAR *)malloc(capacity * sizeof(WCHAR));
    if (resolved == NULL) {
        return -1;
    }
    length = GetFullPathNameW(path, (DWORD)capacity, resolved, NULL);
    if (length == 0 || length >= capacity) {
        goto cleanup;
    }
    for (index = 0; index < (size_t)length; ++index) {
        if (resolved[index] == L'/') {
            resolved[index] = L'\\';
        }
    }
    root_length = windows_root_length(resolved);
    while ((size_t)length > root_length && length > 0 &&
           resolved[length - 1U] == L'\\') {
        resolved[--length] = L'\0';
    }

    if (resolved[0] == L'\\' && resolved[1] == L'\\') {
        suffix = resolved + 2;
        prefix_length = wcslen(extended_unc_prefix);
        suffix_length = wcslen(suffix);
        if (prefix_length + suffix_length >= capacity) {
            goto cleanup;
        }
        memcpy(output, extended_unc_prefix,
               prefix_length * sizeof(WCHAR));
    } else {
        suffix = resolved;
        prefix_length = wcslen(extended_drive_prefix);
        suffix_length = wcslen(suffix);
        if (prefix_length + suffix_length >= capacity) {
            goto cleanup;
        }
        memcpy(output, extended_drive_prefix,
               prefix_length * sizeof(WCHAR));
    }
    memcpy(output + prefix_length, suffix,
           (suffix_length + 1U) * sizeof(WCHAR));
    status = 0;

cleanup:
    free(resolved);
    return status;
#else
    char current_directory[PATH_MAX];
    char *resolved;
    int written;

    if (path == NULL || path[0] == '\0' || output == NULL || capacity == 0U) {
        return -1;
    }
    resolved = realpath(path, NULL);
    if (resolved != NULL) {
        int status = copy_path(output, capacity, resolved);
        free(resolved);
        return status;
    }
    if (path[0] == '/') {
        return copy_path(output, capacity, path);
    }
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        return -1;
    }
    written = snprintf(output, capacity, "%s/%s", current_directory, path);
    return written >= 0 && (size_t)written < capacity ? 0 : -1;
#endif
}

int zip_paths_equal(const zip_path_char *left, const zip_path_char *right)
{
    if (left == NULL || right == NULL) {
        return 0;
    }
#ifdef _WIN32
    return CompareStringOrdinal(left, -1, right, -1, TRUE) == CSTR_EQUAL;
#else
    return strcmp(left, right) == 0;
#endif
}

enum zip_path_kind zip_path_get_kind(const zip_path_char *path)
{
    if (path == NULL) {
        return ZIP_PATH_MISSING;
    }
#ifdef _WIN32
    DWORD attributes = GetFileAttributesW(path);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return ZIP_PATH_MISSING;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return ZIP_PATH_LINK;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return ZIP_PATH_DIRECTORY;
    }
    return ZIP_PATH_FILE;
#else
    struct stat status;

    if (lstat(path, &status) != 0) {
        return ZIP_PATH_MISSING;
    }
    if (S_ISLNK(status.st_mode)) {
        return ZIP_PATH_LINK;
    }
    if (S_ISDIR(status.st_mode)) {
        return ZIP_PATH_DIRECTORY;
    }
    if (S_ISREG(status.st_mode)) {
        return ZIP_PATH_FILE;
    }
    return ZIP_PATH_OTHER;
#endif
}

int zip_path_get_file_size(const zip_path_char *path, uint64_t *size)
{
    if (path == NULL || size == NULL) {
        return -1;
    }
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER length;

    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
        return -1;
    }
    length.HighPart = data.nFileSizeHigh;
    length.LowPart = data.nFileSizeLow;
    *size = (uint64_t)length.QuadPart;
    return 0;
#else
    struct stat status;

    if (stat(path, &status) != 0 || status.st_size < 0) {
        return -1;
    }
    *size = (uint64_t)status.st_size;
    return 0;
#endif
}

zip_path_char *zip_path_join(const zip_path_char *parent,
                             const zip_path_char *name,
                             zip_path_char separator)
{
    size_t parent_length;
    size_t name_length;
    int needs_separator;
    size_t total;
    zip_path_char *joined = NULL;

    if (parent == NULL || name == NULL) {
        return NULL;
    }
    parent_length = zip_path_length(parent);
    name_length = zip_path_length(name);
    needs_separator = parent_length > 0 &&
                      parent[parent_length - 1U] != (zip_path_char)'/' &&
                      parent[parent_length - 1U] != (zip_path_char)'\\';
    if (name_length > SIZE_MAX - (size_t)needs_separator - 1U ||
        parent_length > SIZE_MAX - name_length -
                            (size_t)needs_separator - 1U) {
        return NULL;
    }
    total = parent_length + (size_t)needs_separator + name_length + 1U;
    if (total > SIZE_MAX / sizeof(zip_path_char)) {
        return NULL;
    }
    joined = (zip_path_char *)malloc(total * sizeof(zip_path_char));

    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, parent, parent_length * sizeof(zip_path_char));
    if (needs_separator) {
        joined[parent_length++] = separator;
    }
    memcpy(joined + parent_length, name,
           (name_length + 1U) * sizeof(zip_path_char));
    return joined;
}

const zip_path_char *zip_path_basename(const zip_path_char *path)
{
    const zip_path_char *cursor;
    const zip_path_char *base = path;

    if (path == NULL) {
        return NULL;
    }

    for (cursor = path; *cursor != (zip_path_char)'\0'; ++cursor) {
        if (*cursor == (zip_path_char)'/' ||
            *cursor == (zip_path_char)'\\') {
            base = cursor + 1;
        }
    }
    return base;
}

char *zip_path_to_utf8(const zip_path_char *path, int for_display)
{
    if (path == NULL) {
        return NULL;
    }
#ifdef _WIN32
    const WCHAR *source = path;
    char *converted;
    int required;
    int unc_display = 0;

    if (for_display && wcsncmp(source, L"\\\\?\\UNC\\", 8U) == 0) {
        source += 8;
        unc_display = 1;
    } else if (for_display && wcsncmp(source, L"\\\\?\\", 4U) == 0) {
        source += 4;
    }
    required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
                                   NULL, 0, NULL, NULL);
    if (required <= 0) {
        return NULL;
    }
    converted = (char *)malloc((size_t)required + (size_t)(unc_display * 2));
    if (converted == NULL) {
        return NULL;
    }
    if (unc_display) {
        converted[0] = '\\';
        converted[1] = '\\';
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
                            converted + unc_display * 2, required,
                            NULL, NULL) <= 0) {
        free(converted);
        return NULL;
    }
    return converted;
#else
    char *copy;
    size_t length;

    (void)for_display;
    length = strlen(path);
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, path, length + 1U);
    }
    return copy;
#endif
}
