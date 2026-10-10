#include "unzip_archive.h"
#include "unzip_name.h"
#include "miniz.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define UNZIP_MAX_ENTRIES 100000U
#define UNZIP_MAX_BYTES (8ULL * 1024ULL * 1024ULL * 1024ULL)

typedef struct archive_reader {
    HANDLE handle;
} archive_reader;

typedef struct output_writer {
    HANDLE handle;
    mz_uint64 offset;
} output_writer;

static size_t read_callback(void *opaque, mz_uint64 offset,
                            void *buffer, size_t length)
{
    archive_reader *reader = (archive_reader *)opaque;
    LARGE_INTEGER position;
    DWORD read = 0;

    if (offset > INT64_MAX || length > 0xffffffffU) {
        return 0;
    }
    position.QuadPart = (LONGLONG)offset;
    if (!SetFilePointerEx(reader->handle, position, NULL, FILE_BEGIN) ||
        !ReadFile(reader->handle, buffer, (DWORD)length, &read, NULL)) {
        return 0;
    }
    return read;
}

static size_t write_callback(void *opaque, mz_uint64 offset,
                             const void *buffer, size_t length)
{
    output_writer *writer = (output_writer *)opaque;
    DWORD written = 0;

    if (offset != writer->offset || length > 0xffffffffU ||
        !WriteFile(writer->handle, buffer, (DWORD)length, &written, NULL) ||
        written != length) {
        return 0;
    }
    writer->offset += written;
    return written;
}

static int unicode_name(const char *name, WCHAR **output)
{
    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       name, -1, NULL, 0);
    WCHAR *wide;
    int index;

    if (required <= 1) {
        return -1;
    }
    wide = (WCHAR *)malloc((size_t)required * sizeof(WCHAR));
    if (wide == NULL ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            name, -1, wide, required) != required) {
        free(wide);
        return -1;
    }
    for (index = 0; index < required - 1; ++index) {
        if (wide[index] == L'/') {
            wide[index] = L'\\';
        }
    }
    *output = wide;
    return 0;
}

static int safe_directory(const WCHAR *path)
{
    DWORD attributes;

    if (CreateDirectoryW(path, NULL)) {
        return 0;
    }
    attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 ? 0 : -1;
}

static int create_parents(WCHAR *path, size_t root_length)
{
    size_t index;

    for (index = root_length + 1U; path[index] != L'\0'; ++index) {
        if (path[index] == L'\\') {
            path[index] = L'\0';
            if (safe_directory(path) != 0) {
                path[index] = L'\\';
                return -1;
            }
            path[index] = L'\\';
        }
    }
    return 0;
}

static int entry_name(mz_zip_archive *archive, mz_uint index,
                      mz_zip_archive_file_stat *stat, char **name_out,
                      WCHAR **wide_out, unzip_result *result)
{
    mz_uint required;
    char *name = NULL;
    WCHAR *wide = NULL;
    size_t index_byte;
    unsigned int unix_type;

    if (!mz_zip_reader_file_stat(archive, index, stat)) {
        snprintf(result->error, sizeof(result->error),
                 "cannot inspect archive entry %u", index);
        return -1;
    }
    required = mz_zip_reader_get_filename(archive, index, NULL, 0);
    if (required < 2U || required > 65536U) {
        snprintf(result->error, sizeof(result->error),
                 "invalid archive entry name at index %u", index);
        return -1;
    }
    name = (char *)malloc(required);
    if (name == NULL ||
        mz_zip_reader_get_filename(archive, index, name, required) != required ||
        strlen(name) != (size_t)required - 1U ||
        !unzip_name_validate(name, (size_t)required - 1U,
                             stat->m_is_directory)) {
        snprintf(result->error, sizeof(result->error),
                 "unsafe archive entry name at index %u", index);
        free(name);
        return -1;
    }
    for (index_byte = 0; index_byte + 1U < required; ++index_byte) {
        if ((unsigned char)name[index_byte] >= 0x80U &&
            (stat->m_bit_flag & 0x0800U) == 0U) {
            snprintf(result->error, sizeof(result->error),
                     "non-ASCII entry lacks UTF-8 flag at index %u", index);
            free(name);
            return -1;
        }
    }
    unix_type = (unsigned int)((stat->m_external_attr >> 16U) & 0170000U);
    if ((unix_type != 0U && unix_type != 0100000U &&
         unix_type != 0040000U) ||
        (unix_type == 0040000U && !stat->m_is_directory) ||
        (unix_type == 0100000U && stat->m_is_directory) ||
        (stat->m_is_directory && stat->m_uncomp_size != 0U) ||
        (stat->m_external_attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
        stat->m_is_encrypted || !stat->m_is_supported) {
        snprintf(result->error, sizeof(result->error),
                 "unsupported archive entry at index %u", index);
        free(name);
        return -1;
    }
    if (unicode_name(name, &wide) != 0) {
        snprintf(result->error, sizeof(result->error),
                 "invalid Unicode entry at index %u", index);
        free(name);
        return -1;
    }
    *name_out = name;
    *wide_out = wide;
    return 0;
}

int unzip_extract_archive(const zip_path_char *archive_path,
                          const zip_path_char *destination_path,
                          unzip_result *result)
{
    WCHAR *source = NULL;
    WCHAR *destination = NULL;
    archive_reader reader;
    mz_zip_archive archive;
    uint64_t archive_size;
    uint64_t total_bytes = 0;
    mz_uint count = 0;
    mz_uint index;
    size_t root_length;
    int initialized = 0;
    int status = -1;

    if (result == NULL) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    memset(&archive, 0, sizeof(archive));
    reader.handle = INVALID_HANDLE_VALUE;
    if (archive_path == NULL || destination_path == NULL ||
        archive_path[0] == L'\0' || destination_path[0] == L'\0') {
        snprintf(result->error, sizeof(result->error), "paths are required");
        goto cleanup;
    }
    source = (WCHAR *)malloc(ZIP_PATH_CAPACITY * sizeof(WCHAR));
    destination = (WCHAR *)malloc(ZIP_PATH_CAPACITY * sizeof(WCHAR));
    if (source == NULL || destination == NULL ||
        zip_path_resolve(archive_path, source, ZIP_PATH_CAPACITY) != 0 ||
        zip_path_resolve(destination_path, destination, ZIP_PATH_CAPACITY) != 0) {
        snprintf(result->error, sizeof(result->error), "cannot resolve paths");
        goto cleanup;
    }
    if (zip_path_get_kind(source) != ZIP_PATH_FILE ||
        zip_path_get_file_size(source, &archive_size) != 0 ||
        archive_size > INT64_MAX) {
        snprintf(result->error, sizeof(result->error),
                 "archive is missing, linked, or too large");
        goto cleanup;
    }
    if (zip_path_get_kind(destination) != ZIP_PATH_MISSING) {
        snprintf(result->error, sizeof(result->error),
                 "destination must not already exist");
        goto cleanup;
    }
    root_length = wcslen(destination);
    reader.handle = CreateFileW(source, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (reader.handle == INVALID_HANDLE_VALUE) {
        snprintf(result->error, sizeof(result->error), "cannot open archive");
        goto cleanup;
    }
    archive.m_pRead = read_callback;
    archive.m_pIO_opaque = &reader;
    if (!mz_zip_reader_init(&archive, archive_size, 0)) {
        snprintf(result->error, sizeof(result->error),
                 "invalid ZIP archive: %s",
                 mz_zip_get_error_string(mz_zip_get_last_error(&archive)));
        goto cleanup;
    }
    initialized = 1;
    count = mz_zip_reader_get_num_files(&archive);
    if (count > UNZIP_MAX_ENTRIES) {
        snprintf(result->error, sizeof(result->error),
                 "archive exceeds %u-entry limit", UNZIP_MAX_ENTRIES);
        goto cleanup;
    }
    /* Check every entry before creating any output. */
    for (index = 0; index < count; ++index) {
        mz_zip_archive_file_stat entry;
        char *name = NULL;
        WCHAR *wide = NULL;

        if (entry_name(&archive, index, &entry, &name, &wide, result) != 0) {
            goto cleanup;
        }
        if (root_length + 1U + wcslen(wide) >= ZIP_PATH_CAPACITY ||
            entry.m_uncomp_size > UNZIP_MAX_BYTES - total_bytes) {
            snprintf(result->error, sizeof(result->error),
                     "entry path or total output exceeds limit");
            free(name);
            free(wide);
            goto cleanup;
        }
        total_bytes += entry.m_uncomp_size;
        free(name);
        free(wide);
    }
    if (!CreateDirectoryW(destination, NULL)) {
        snprintf(result->error, sizeof(result->error),
                 "cannot create destination directory");
        goto cleanup;
    }
    for (index = 0; index < count; ++index) {
        mz_zip_archive_file_stat entry;
        char *name = NULL;
        WCHAR *wide = NULL;
        WCHAR *target = NULL;

        if (entry_name(&archive, index, &entry, &name, &wide, result) != 0) {
            goto cleanup;
        }
        if (entry.m_is_directory) {
            wide[wcslen(wide) - 1U] = L'\0';
        }
        target = zip_path_join(destination, wide, L'\\');
        free(wide);
        if (target == NULL || create_parents(target, root_length) != 0) {
            snprintf(result->error, sizeof(result->error),
                     "cannot create parent for '%s'", name);
            free(name);
            free(target);
            goto cleanup;
        }
        if (entry.m_is_directory) {
            if (safe_directory(target) != 0) {
                snprintf(result->error, sizeof(result->error),
                         "cannot create directory '%s'", name);
                free(name);
                free(target);
                goto cleanup;
            }
            ++result->directories;
        } else {
            output_writer writer;
            writer.handle = CreateFileW(target, GENERIC_WRITE, 0, NULL,
                                        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            writer.offset = 0;
            if (writer.handle == INVALID_HANDLE_VALUE) {
                snprintf(result->error, sizeof(result->error),
                         "cannot create file '%s'", name);
                free(name);
                free(target);
                goto cleanup;
            }
            if (!mz_zip_reader_extract_to_callback(&archive, index,
                                                    write_callback, &writer, 0) ||
                writer.offset != entry.m_uncomp_size) {
                snprintf(result->error, sizeof(result->error),
                         "cannot extract file '%s'", name);
                (void)CloseHandle(writer.handle);
                (void)DeleteFileW(target);
                free(name);
                free(target);
                goto cleanup;
            }
            {
                BOOL flushed = FlushFileBuffers(writer.handle);
                BOOL closed = CloseHandle(writer.handle);
                if (!flushed || !closed) {
                    snprintf(result->error, sizeof(result->error),
                             "cannot finish file '%s'", name);
                    (void)DeleteFileW(target);
                    free(name);
                    free(target);
                    goto cleanup;
                }
            }
            ++result->files;
            result->bytes += entry.m_uncomp_size;
        }
        free(name);
        free(target);
    }
    status = 0;

cleanup:
    if (initialized || archive.m_pState != NULL) {
        (void)mz_zip_reader_end(&archive);
    }
    if (reader.handle != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(reader.handle);
    }
    free(source);
    free(destination);
    return status;
}

#endif
