#include "zip_archive.h"
#include "miniz.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

typedef struct zip_context {
    mz_zip_archive archive;
    int archive_initialized;
    int compression_level;
    const zip_path_char *destination_absolute;
    const zip_path_char *temporary_absolute;
    uint64_t max_input_bytes;
    uint64_t max_entries;
    zip_result *result;
#ifdef _WIN32
    HANDLE output_file;
    mz_uint64 output_offset;
#endif
} zip_context;

#ifdef _WIN32
typedef struct zip_file_reader {
    HANDLE handle;
    mz_uint64 offset;
    int failed;
} zip_file_reader;

static size_t read_file_callback(void *opaque, mz_uint64 offset,
                                 void *buffer, size_t length)
{
    zip_file_reader *reader = (zip_file_reader *)opaque;
    DWORD read = 0;

    if (reader->failed || offset != reader->offset || length > 0xffffffffU ||
        !ReadFile(reader->handle, buffer, (DWORD)length, &read, NULL)) {
        reader->failed = 1;
        return 0;
    }
    reader->offset += read;
    return read;
}

static size_t write_file_callback(void *opaque, mz_uint64 offset,
                                  const void *buffer, size_t length)
{
    zip_context *context = (zip_context *)opaque;
    DWORD written = 0;

    if (offset != context->output_offset || length > 0xffffffffU ||
        !WriteFile(context->output_file, buffer, (DWORD)length,
                   &written, NULL)) {
        return 0;
    }
    context->output_offset += written;
    return written;
}
#endif

static void trim_incomplete_utf8(char *text)
{
    size_t length = strlen(text);
    size_t start;
    size_t actual;
    size_t expected;
    unsigned char lead;

    if (length == 0U) {
        return;
    }
    start = length - 1U;
    while (start > 0U &&
           ((unsigned char)text[start] & 0xc0U) == 0x80U) {
        --start;
    }
    lead = (unsigned char)text[start];
    actual = length - start;
    if (lead < 0x80U) {
        expected = 1U;
    } else if ((lead & 0xe0U) == 0xc0U) {
        expected = 2U;
    } else if ((lead & 0xf0U) == 0xe0U) {
        expected = 3U;
    } else if ((lead & 0xf8U) == 0xf0U) {
        expected = 4U;
    } else {
        text[start] = '\0';
        return;
    }
    if (actual < expected) {
        text[start] = '\0';
    }
}

static void set_error(zip_result *result, const char *format, ...)
{
    va_list arguments;

    if (result == NULL || result->error[0] != '\0') {
        return;
    }

    va_start(arguments, format);
    (void)vsnprintf(result->error, sizeof(result->error), format, arguments);
    va_end(arguments);
    trim_incomplete_utf8(result->error);
}

static char *join_archive_path(const char *parent, const char *name)
{
    size_t parent_length = strlen(parent);
    size_t name_length = strlen(name);
    int needs_separator = parent_length > 0 &&
                          parent[parent_length - 1U] != '/';
    size_t total = parent_length + (size_t)needs_separator + name_length + 1U;
    char *joined = (char *)malloc(total);

    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, parent, parent_length);
    if (needs_separator) {
        joined[parent_length++] = '/';
    }
    memcpy(joined + parent_length, name, name_length + 1U);
    return joined;
}

static void set_path_error(zip_result *result, const char *operation,
                           const zip_path_char *path, const char *detail)
{
    char *display = zip_path_to_utf8(path, 1);

    if (display == NULL) {
        set_error(result, "%s a path that cannot be represented as UTF-8%s%s",
                  operation, detail != NULL ? ": " : "",
                  detail != NULL ? detail : "");
        return;
    }
    set_error(result, "%s '%s'%s%s", operation, display,
              detail != NULL ? ": " : "", detail != NULL ? detail : "");
    free(display);
}

static int path_is_output(zip_context *context,
                          const zip_path_char *absolute_path_value)
{
    return (context->destination_absolute != NULL &&
            zip_paths_equal(absolute_path_value,
                            context->destination_absolute)) ||
           (context->temporary_absolute != NULL &&
            zip_paths_equal(absolute_path_value,
                            context->temporary_absolute));
}

static void set_miniz_error(zip_context *context, const char *operation,
                            const zip_path_char *path)
{
    mz_zip_error error = mz_zip_get_last_error(&context->archive);

    set_path_error(context->result, operation, path,
                   mz_zip_get_error_string(error));
}

static void set_miniz_archive_error(zip_context *context,
                                    const char *operation,
                                    const char *archive_path)
{
    mz_zip_error error = mz_zip_get_last_error(&context->archive);

    set_error(context->result, "%s '%s': %s", operation, archive_path,
              mz_zip_get_error_string(error));
}

static int add_directory_entry(zip_context *context,
                               const char *archive_path)
{
    size_t length = strlen(archive_path);
    char *entry;
    int success;

    if (context->max_entries != 0 &&
        context->result->files + context->result->directories >=
            context->max_entries) {
        set_error(context->result,
                  "in-memory entry limit exceeded (%llu entries); "
                  "rerun with --disk",
                  (unsigned long long)context->max_entries);
        return -1;
    }
    if (length > (size_t)0xffffffffU - 2U) {
        set_error(context->result, "archive path is too long: '%s'",
                  archive_path);
        return -1;
    }
    entry = (char *)malloc(length + 2U);
    if (entry == NULL) {
        set_error(context->result, "out of memory while adding '%s'",
                  archive_path);
        return -1;
    }
    memcpy(entry, archive_path, length);
    if (length == 0 || entry[length - 1U] != '/') {
        entry[length++] = '/';
    }
    entry[length] = '\0';

    success = mz_zip_writer_add_mem(&context->archive, entry, NULL, 0,
                                    MZ_NO_COMPRESSION);
    if (!success) {
        set_miniz_archive_error(context, "cannot add directory", entry);
        free(entry);
        return -1;
    }

    context->result->directories++;
    free(entry);
    return 0;
}

static int add_file_entry(zip_context *context,
                          const zip_path_char *disk_path,
                          const char *archive_path)
{
#ifndef _WIN32
    char *disk_path_utf8;
#else
    zip_file_reader reader;
    HANDLE file;
#endif
    uint64_t size;

    if (path_is_output(context, disk_path)) {
        return 0;
    }
    if (zip_path_get_file_size(disk_path, &size) != 0) {
        set_path_error(context->result, "cannot stat", disk_path, NULL);
        return -1;
    }
    if (context->max_entries != 0 &&
        context->result->files + context->result->directories >=
            context->max_entries) {
        set_error(context->result,
                  "in-memory entry limit exceeded (%llu entries); "
                  "rerun with --disk",
                  (unsigned long long)context->max_entries);
        return -1;
    }
    if (context->max_input_bytes != 0 &&
        (context->result->bytes > context->max_input_bytes ||
         size > context->max_input_bytes - context->result->bytes)) {
        set_error(context->result,
                  "in-memory input limit exceeded (%llu MiB); "
                  "rerun with --disk",
                  (unsigned long long)(context->max_input_bytes /
                                      (1024ULL * 1024ULL)));
        return -1;
    }
#ifdef _WIN32
    file = CreateFileW(disk_path, GENERIC_READ, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        set_path_error(context->result, "cannot open", disk_path, NULL);
        return -1;
    }
    reader.handle = file;
    reader.offset = 0;
    reader.failed = 0;
    if (!mz_zip_writer_add_read_buf_callback(
            &context->archive, archive_path, read_file_callback, &reader,
            size, NULL, NULL, 0, (mz_uint)context->compression_level,
            NULL, 0, NULL, 0)) {
        set_miniz_error(context, "cannot add file", disk_path);
        (void)CloseHandle(file);
        return -1;
    }
    if (reader.failed || reader.offset != size) {
        set_path_error(context->result, "file changed while reading",
                       disk_path, NULL);
        (void)CloseHandle(file);
        return -1;
    }
    (void)CloseHandle(file);
#else
    disk_path_utf8 = zip_path_to_utf8(disk_path, 0);
    if (disk_path_utf8 == NULL) {
        set_path_error(context->result, "cannot encode", disk_path,
                       "path is not valid Unicode");
        return -1;
    }
    /* miniz treats archive names as UTF-8 unless ASCII_FILENAME is set. */
    if (!mz_zip_writer_add_file(&context->archive, archive_path,
                                disk_path_utf8, NULL, 0,
                                (mz_uint)context->compression_level)) {
        set_miniz_error(context, "cannot add file", disk_path);
        free(disk_path_utf8);
        return -1;
    }
    free(disk_path_utf8);
#endif
    context->result->files++;
    context->result->bytes += size;
    return 0;
}

static int walk_directory(zip_context *context,
                          const zip_path_char *disk_path,
                          const char *archive_path);

static int process_child(zip_context *context,
                         const zip_path_char *disk_parent,
                         const char *archive_parent,
                         const zip_path_char *name,
                         enum zip_path_kind kind)
{
#ifdef _WIN32
    const zip_path_char disk_separator = L'\\';
#else
    const zip_path_char disk_separator = '/';
#endif
    zip_path_char *child_disk;
    char *name_utf8;
    char *child_archive;
    int status = 0;

    if (kind == ZIP_PATH_LINK) {
        context->result->skipped_links++;
        return 0;
    }
    if (kind != ZIP_PATH_FILE && kind != ZIP_PATH_DIRECTORY) {
        return 0;
    }

    child_disk = zip_path_join(disk_parent, name, disk_separator);
    name_utf8 = zip_path_to_utf8(name, 0);
    child_archive = name_utf8 != NULL
                        ? join_archive_path(archive_parent, name_utf8)
                        : NULL;
    if (child_disk == NULL || child_archive == NULL) {
        if (name_utf8 == NULL) {
            set_path_error(context->result, "cannot encode", name,
                           "filename is not valid Unicode");
        } else {
            set_path_error(context->result,
                           "out of memory while visiting", name, NULL);
        }
        free(child_disk);
        free(name_utf8);
        free(child_archive);
        return -1;
    }

    if (kind == ZIP_PATH_DIRECTORY) {
        if (add_directory_entry(context, child_archive) != 0) {
            status = -1;
        } else {
            status = walk_directory(context, child_disk, child_archive);
        }
    } else {
        status = add_file_entry(context, child_disk, child_archive);
    }

    free(child_disk);
    free(name_utf8);
    free(child_archive);
    return status;
}

static int walk_directory(zip_context *context,
                          const zip_path_char *disk_path,
                          const char *archive_path)
{
#ifdef _WIN32
    zip_path_char *pattern = zip_path_join(disk_path, L"*", L'\\');
    WIN32_FIND_DATAW found;
    HANDLE search;
    int status = 0;

    if (pattern == NULL) {
        set_path_error(context->result, "out of memory while visiting",
                       disk_path, NULL);
        return -1;
    }
    search = FindFirstFileW(pattern, &found);
    free(pattern);
    if (search == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return 0;
        }
        char detail[64];
        (void)snprintf(detail, sizeof(detail), "Windows error %lu",
                       (unsigned long)error);
        set_path_error(context->result, "cannot enumerate", disk_path,
                       detail);
        return -1;
    }

    do {
        enum zip_path_kind kind;

        if (wcscmp(found.cFileName, L".") == 0 ||
            wcscmp(found.cFileName, L"..") == 0) {
            continue;
        }
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            kind = ZIP_PATH_LINK;
        } else if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            kind = ZIP_PATH_DIRECTORY;
        } else {
            kind = ZIP_PATH_FILE;
        }
        if (process_child(context, disk_path, archive_path, found.cFileName,
                          kind) != 0) {
            status = -1;
            break;
        }
    } while (FindNextFileW(search, &found));

    if (status == 0 && GetLastError() != ERROR_NO_MORE_FILES) {
        char detail[64];
        (void)snprintf(detail, sizeof(detail), "Windows error %lu",
                       (unsigned long)GetLastError());
        set_path_error(context->result, "cannot enumerate", disk_path,
                       detail);
        status = -1;
    }
    (void)FindClose(search);
    return status;
#else
    DIR *directory = opendir(disk_path);
    struct dirent *entry;
    int status = 0;

    if (directory == NULL) {
        set_error(context->result, "cannot enumerate '%s': %s", disk_path,
                  strerror(errno));
        return -1;
    }

    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        zip_path_char *child;
        enum zip_path_kind kind;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        child = zip_path_join(disk_path, entry->d_name, '/');
        if (child == NULL) {
            set_error(context->result, "out of memory while visiting '%s'",
                      entry->d_name);
            status = -1;
            break;
        }
        kind = zip_path_get_kind(child);
        free(child);
        if (process_child(context, disk_path, archive_path, entry->d_name,
                          kind) != 0) {
            status = -1;
            break;
        }
        errno = 0;
    }
    if (status == 0 && errno != 0) {
        set_error(context->result, "cannot enumerate '%s': %s", disk_path,
                  strerror(errno));
        status = -1;
    }
    (void)closedir(directory);
    return status;
#endif
}

static int make_temporary_path(const zip_path_char *destination,
                               zip_path_char *temporary,
                               size_t capacity)
{
    int written;
#ifdef _WIN32
    unsigned long process_id = (unsigned long)
#ifdef ZIP_BOF
                               GetCurrentProcessId();
#else
                               _getpid();
#endif
    unsigned long nonce = (unsigned long)GetTickCount();

    written = _snwprintf(temporary, capacity, L"%ls.tmp.%lu.%lu",
                         destination, process_id, nonce);
#else
    unsigned long process_id = (unsigned long)getpid();
    unsigned long nonce = (unsigned long)time(NULL);

    written = snprintf(temporary, capacity, "%s.tmp.%lu.%lu", destination,
                       process_id, nonce);
#endif
    return written >= 0 && (size_t)written < capacity ? 0 : -1;
}

static int replace_destination(const zip_path_char *temporary,
                               const zip_path_char *destination,
                               zip_result *result)
{
#ifdef _WIN32
    if (!MoveFileExW(temporary, destination,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        char detail[64];
        (void)snprintf(detail, sizeof(detail), "Windows error %lu",
                       (unsigned long)GetLastError());
        set_path_error(result, "cannot move archive to", destination, detail);
        return -1;
    }
#else
    if (rename(temporary, destination) != 0) {
        set_error(result, "cannot move archive to '%s': %s", destination,
                  strerror(errno));
        return -1;
    }
#endif
    return 0;
}

int zip_create_archive(const zip_path_char *source_path,
                       const zip_path_char *destination_path,
                       int compression_level,
                       zip_result *result)
{
    zip_context context;
    enum zip_path_kind source_kind;
    zip_path_char *source_absolute = NULL;
    zip_path_char *destination_absolute = NULL;
    zip_path_char *temporary_path = NULL;
    const zip_path_char *root_component;
    char *root_name = NULL;
#ifndef _WIN32
    char *temporary_path_utf8 = NULL;
#endif
    int status = -1;

    if (result == NULL) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    memset(&context, 0, sizeof(context));
    context.compression_level = compression_level;
    context.result = result;

    if (source_path == NULL || source_path[0] == (zip_path_char)'\0' ||
        destination_path == NULL ||
        destination_path[0] == (zip_path_char)'\0') {
        set_error(result, "source and destination paths are required");
        goto cleanup;
    }
    if (compression_level < 0 || compression_level > 9) {
        set_error(result, "compression level must be between 0 and 9");
        goto cleanup;
    }

    source_absolute = (zip_path_char *)malloc(
        ZIP_PATH_CAPACITY * sizeof(zip_path_char));
    destination_absolute = (zip_path_char *)malloc(
        ZIP_PATH_CAPACITY * sizeof(zip_path_char));
    temporary_path = (zip_path_char *)malloc(
        ZIP_PATH_CAPACITY * sizeof(zip_path_char));
    if (source_absolute == NULL || destination_absolute == NULL ||
        temporary_path == NULL) {
        set_error(result, "out of memory while preparing paths");
        goto cleanup;
    }

    if (zip_path_resolve(source_path, source_absolute,
                         ZIP_PATH_CAPACITY) != 0) {
        set_path_error(result, "cannot resolve source path", source_path,
                       "unsupported, ambiguous, or too long");
        goto cleanup;
    }
    source_kind = zip_path_get_kind(source_absolute);
    if (source_kind == ZIP_PATH_MISSING) {
        set_path_error(result, "source path does not exist", source_path,
                       NULL);
        goto cleanup;
    }
    if (source_kind == ZIP_PATH_LINK) {
        set_path_error(result,
                       "source path is a symbolic link or reparse point",
                       source_path, NULL);
        goto cleanup;
    }
    if (source_kind != ZIP_PATH_FILE &&
        source_kind != ZIP_PATH_DIRECTORY) {
        set_path_error(result,
                       "source path is not a regular file or directory",
                       source_path, NULL);
        goto cleanup;
    }

    if (zip_path_resolve(destination_path, destination_absolute,
                         ZIP_PATH_CAPACITY) != 0) {
        set_path_error(result, "cannot resolve destination path",
                       destination_path,
                       "unsupported, ambiguous, or too long");
        goto cleanup;
    }
    if (zip_paths_equal(source_absolute, destination_absolute)) {
        set_error(result, "source and destination paths must differ");
        goto cleanup;
    }
    if (make_temporary_path(destination_absolute, temporary_path,
                            ZIP_PATH_CAPACITY) != 0) {
        set_path_error(result, "destination path is too long",
                       destination_path, NULL);
        goto cleanup;
    }

    context.destination_absolute = destination_absolute;
    context.temporary_absolute = temporary_path;
#ifdef _WIN32
    context.output_file = CreateFileW(temporary_path, GENERIC_WRITE, 0,
                                      NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                                      NULL);
    if (context.output_file == INVALID_HANDLE_VALUE) {
        set_path_error(result, "cannot create archive", temporary_path, NULL);
        goto cleanup;
    }
    context.archive.m_pWrite = write_file_callback;
    context.archive.m_pIO_opaque = &context;
    if (!mz_zip_writer_init(&context.archive, 0)) {
        set_miniz_error(&context, "cannot create archive", destination_path);
        goto cleanup;
    }
#else
    temporary_path_utf8 = zip_path_to_utf8(temporary_path, 0);
    if (temporary_path_utf8 == NULL) {
        set_path_error(result, "cannot encode destination path",
                       destination_path, "path is not valid Unicode");
        goto cleanup;
    }
    MZ_CLEAR_OBJ(context.archive);
    if (!mz_zip_writer_init_file(&context.archive, temporary_path_utf8, 0)) {
        set_miniz_error(&context, "cannot create archive", destination_path);
        goto cleanup;
    }
#endif
    context.archive_initialized = 1;

    root_component = zip_path_basename(source_absolute);
    if (*root_component == (zip_path_char)'\0') {
#ifdef _WIN32
        root_component = L"archive";
#else
        root_component = "archive";
#endif
    }
    root_name = zip_path_to_utf8(root_component, 0);
    if (root_name == NULL) {
        set_path_error(result, "cannot encode source basename", source_path,
                       "filename is not valid Unicode");
        goto cleanup;
    }
    if (source_kind == ZIP_PATH_DIRECTORY) {
        if (add_directory_entry(&context, root_name) != 0 ||
            walk_directory(&context, source_absolute, root_name) != 0) {
            goto cleanup;
        }
    } else if (add_file_entry(&context, source_absolute, root_name) != 0) {
        goto cleanup;
    }

    if (!mz_zip_writer_finalize_archive(&context.archive)) {
        set_miniz_error(&context, "cannot finalize archive", destination_path);
        goto cleanup;
    }
    if (!mz_zip_writer_end(&context.archive)) {
        context.archive_initialized = 0;
        set_path_error(result, "cannot close archive", destination_path,
                       NULL);
        goto cleanup;
    }
    context.archive_initialized = 0;

#ifdef _WIN32
    if (!FlushFileBuffers(context.output_file)) {
        set_path_error(result, "cannot close archive", destination_path,
                       NULL);
        goto cleanup;
    }
    if (!CloseHandle(context.output_file)) {
        context.output_file = INVALID_HANDLE_VALUE;
        set_path_error(result, "cannot close archive", destination_path,
                       NULL);
        goto cleanup;
    }
    context.output_file = INVALID_HANDLE_VALUE;
#endif

    if (replace_destination(temporary_path, destination_absolute, result) != 0) {
        goto cleanup;
    }
    status = 0;

cleanup:
    if (context.archive_initialized) {
        (void)mz_zip_writer_end(&context.archive);
    }
#ifdef _WIN32
    if (context.output_file != NULL &&
        context.output_file != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(context.output_file);
    }
#endif
    if (status != 0 && temporary_path != NULL) {
#ifdef _WIN32
        (void)DeleteFileW(temporary_path);
#else
        (void)remove(temporary_path);
#endif
    }
    free(source_absolute);
    free(destination_absolute);
    free(temporary_path);
#ifndef _WIN32
    free(temporary_path_utf8);
#endif
    free(root_name);
    return status;
}

int zip_create_archive_memory(const zip_path_char *source_path,
                              const zip_path_char *excluded_output_path,
                              int compression_level,
                              void **archive_data,
                              size_t *archive_size,
                              zip_result *result)
{
    zip_context context;
    enum zip_path_kind source_kind;
    zip_path_char *source_absolute = NULL;
    zip_path_char *output_absolute = NULL;
    const zip_path_char *root_component;
    char *root_name = NULL;
    int status = -1;

    if (archive_data != NULL) {
        *archive_data = NULL;
    }
    if (archive_size != NULL) {
        *archive_size = 0;
    }
    if (result == NULL) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    memset(&context, 0, sizeof(context));
    context.compression_level = compression_level;
    context.max_input_bytes = ZIP_MEMORY_INPUT_LIMIT;
    context.max_entries = ZIP_MEMORY_ENTRY_LIMIT;
    context.result = result;

    if (archive_data == NULL || archive_size == NULL) {
        set_error(result, "archive output pointers are required");
        goto cleanup;
    }
    if (source_path == NULL || source_path[0] == (zip_path_char)'\0') {
        set_error(result, "source path is required");
        goto cleanup;
    }
    if (compression_level < 0 || compression_level > 9) {
        set_error(result, "compression level must be between 0 and 9");
        goto cleanup;
    }

    source_absolute = (zip_path_char *)malloc(
        ZIP_PATH_CAPACITY * sizeof(zip_path_char));
    output_absolute = (zip_path_char *)malloc(
        ZIP_PATH_CAPACITY * sizeof(zip_path_char));
    if (source_absolute == NULL || output_absolute == NULL) {
        set_error(result, "out of memory while preparing paths");
        goto cleanup;
    }
    if (zip_path_resolve(source_path, source_absolute,
                         ZIP_PATH_CAPACITY) != 0) {
        set_path_error(result, "cannot resolve source path", source_path,
                       "unsupported, ambiguous, or too long");
        goto cleanup;
    }
    source_kind = zip_path_get_kind(source_absolute);
    if (source_kind == ZIP_PATH_MISSING) {
        set_path_error(result, "source path does not exist", source_path,
                       NULL);
        goto cleanup;
    }
    if (source_kind == ZIP_PATH_LINK) {
        set_path_error(result,
                       "source path is a symbolic link or reparse point",
                       source_path, NULL);
        goto cleanup;
    }
    if (source_kind != ZIP_PATH_FILE &&
        source_kind != ZIP_PATH_DIRECTORY) {
        set_path_error(result,
                       "source path is not a regular file or directory",
                       source_path, NULL);
        goto cleanup;
    }
    if (excluded_output_path == NULL ||
        excluded_output_path[0] == (zip_path_char)'\0' ||
        zip_path_resolve(excluded_output_path, output_absolute,
                         ZIP_PATH_CAPACITY) != 0) {
        if (excluded_output_path != NULL) {
            set_path_error(result, "cannot resolve output path",
                           excluded_output_path,
                           "unsupported, ambiguous, or too long");
        } else {
            set_error(result, "cannot resolve output path");
        }
        goto cleanup;
    }
    if (zip_paths_equal(source_absolute, output_absolute)) {
        set_error(result, "source and output paths must differ");
        goto cleanup;
    }
    context.destination_absolute = output_absolute;

    MZ_CLEAR_OBJ(context.archive);
    if (!mz_zip_writer_init_heap(&context.archive, 0, 0)) {
        set_miniz_error(&context, "cannot create in-memory archive",
                        source_path);
        goto cleanup;
    }
    context.archive_initialized = 1;

    root_component = zip_path_basename(source_absolute);
    if (*root_component == (zip_path_char)'\0') {
#ifdef _WIN32
        root_component = L"archive";
#else
        root_component = "archive";
#endif
    }
    root_name = zip_path_to_utf8(root_component, 0);
    if (root_name == NULL) {
        set_path_error(result, "cannot encode source basename", source_path,
                       "filename is not valid Unicode");
        goto cleanup;
    }
    if (source_kind == ZIP_PATH_DIRECTORY) {
        if (add_directory_entry(&context, root_name) != 0 ||
            walk_directory(&context, source_absolute, root_name) != 0) {
            goto cleanup;
        }
    } else if (add_file_entry(&context, source_absolute, root_name) != 0) {
        goto cleanup;
    }

    if (!mz_zip_writer_finalize_heap_archive(&context.archive, archive_data,
                                             archive_size)) {
        set_miniz_error(&context, "cannot finalize in-memory archive",
                        source_path);
        goto cleanup;
    }
    if (!mz_zip_writer_end(&context.archive)) {
        context.archive_initialized = 0;
        set_error(result, "cannot close in-memory archive");
        goto cleanup;
    }
    context.archive_initialized = 0;
    status = 0;

cleanup:
    if (context.archive_initialized) {
        (void)mz_zip_writer_end(&context.archive);
    }
    if (status != 0 && archive_data != NULL && *archive_data != NULL) {
        mz_free(*archive_data);
        *archive_data = NULL;
        *archive_size = 0;
    }
    free(source_absolute);
    free(output_absolute);
    free(root_name);
    return status;
}

void zip_free_archive(void *archive_data, size_t archive_size)
{
    volatile unsigned char *cursor = (volatile unsigned char *)archive_data;

    while (cursor != NULL && archive_size-- != 0U) {
        *cursor++ = 0;
    }
    mz_free(archive_data);
}
