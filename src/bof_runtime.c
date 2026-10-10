#include "bof_runtime.h"

#ifdef ZIP_BOF

extern __declspec(dllimport) int __cdecl MSVCRT$_vsnprintf(char *, size_t, const char *, va_list);
extern __declspec(dllimport) int __cdecl MSVCRT$_vsnwprintf(wchar_t *, size_t, const wchar_t *, va_list);

void *bof_malloc(size_t size)
{
    return HeapAlloc(GetProcessHeap(), 0, size != 0 ? size : 1);
}

void *bof_realloc(void *pointer, size_t size)
{
    if (pointer == NULL) {
        return bof_malloc(size);
    }
    return HeapReAlloc(GetProcessHeap(), 0, pointer, size != 0 ? size : 1);
}

void bof_free(void *pointer)
{
    if (pointer != NULL) {
        (void)HeapFree(GetProcessHeap(), 0, pointer);
    }
}

int bof_vsnprintf(char *buffer, size_t capacity, const char *format, va_list args)
{
    int written;

    if (capacity == 0) {
        return -1;
    }
    written = MSVCRT$_vsnprintf(buffer, capacity, format, args);
    buffer[capacity - 1] = '\0';
    return written;
}

int bof_snprintf(char *buffer, size_t capacity, const char *format, ...)
{
    va_list args;
    int written;

    va_start(args, format);
    written = bof_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return written;
}

int bof_snwprintf(WCHAR *buffer, size_t capacity, const WCHAR *format, ...)
{
    va_list args;
    int written;

    if (capacity == 0) {
        return -1;
    }
    va_start(args, format);
    written = MSVCRT$_vsnwprintf(buffer, capacity, format, args);
    va_end(args);
    buffer[capacity - 1] = L'\0';
    return written;
}

/* Clang emits these calls for struct copies and zeroing; forward them to the
 * Beacon-resolved CRT instead of leaving unresolved symbols. */
#undef memcpy
#undef memset

void *memcpy(void *destination, const void *source, size_t size)
{
    return MSVCRT$memcpy(destination, source, size);
}

void *memset(void *destination, int value, size_t size)
{
    return MSVCRT$memset(destination, value, size);
}

#define memcpy MSVCRT$memcpy
#define memset MSVCRT$memset

#endif
