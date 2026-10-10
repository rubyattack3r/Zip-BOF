#include "pe_runtime.h"

/*
 * The headers declare these msvcrt.dll functions as UCRT inline wrappers, so
 * bind them to the msvcrt.dll export names directly.
 */
#define PE_STRING2(value) #value
#define PE_STRING(value) PE_STRING2(value)
#define PE_MSVCRT(name) __asm__(PE_STRING(__USER_LABEL_PREFIX__) #name)

extern int __cdecl msvcrt_vsnprintf(char *, size_t, const char *, va_list)
    PE_MSVCRT(_vsnprintf);
extern int __cdecl msvcrt_vsnwprintf(wchar_t *, size_t, const wchar_t *, va_list)
    PE_MSVCRT(_vsnwprintf);
extern int __cdecl msvcrt_vscprintf(const char *, va_list) PE_MSVCRT(_vscprintf);

/* Unlike C99 vsnprintf, msvcrt does not terminate truncated output. */
int pe_vsnprintf(char *buffer, size_t capacity, const char *format, va_list args)
{
    int written;

    if (capacity == 0) {
        return -1;
    }
    written = msvcrt_vsnprintf(buffer, capacity, format, args);
    buffer[capacity - 1] = '\0';
    return written;
}

int pe_snprintf(char *buffer, size_t capacity, const char *format, ...)
{
    va_list args;
    int written;

    va_start(args, format);
    written = pe_vsnprintf(buffer, capacity, format, args);
    va_end(args);
    return written;
}

int pe_snwprintf(WCHAR *buffer, size_t capacity, const WCHAR *format, ...)
{
    va_list args;
    int written;

    if (capacity == 0) {
        return -1;
    }
    va_start(args, format);
    written = msvcrt_vsnwprintf(buffer, capacity, format, args);
    va_end(args);
    buffer[capacity - 1] = L'\0';
    return written;
}

/* Formats into a heap buffer and writes the bytes to a standard handle. */
int pe_vfprintf(DWORD std_handle, const char *format, va_list args)
{
    va_list measure;
    char *buffer;
    int length;
    DWORD written = 0;
    BOOL ok;

    va_copy(measure, args);
    length = msvcrt_vscprintf(format, measure);
    va_end(measure);
    if (length < 0) {
        return -1;
    }
    buffer = (char *)malloc((size_t)length + 1U);
    if (buffer == NULL) {
        return -1;
    }
    (void)msvcrt_vsnprintf(buffer, (size_t)length + 1U, format, args);
    ok = WriteFile(GetStdHandle(std_handle), buffer, (DWORD)length, &written,
                   NULL);
    free(buffer);
    return ok && written == (DWORD)length ? length : -1;
}

int pe_fprintf(DWORD std_handle, const char *format, ...)
{
    va_list args;
    int written;

    va_start(args, format);
    written = pe_vfprintf(std_handle, format, args);
    va_end(args);
    return written;
}

#ifdef BUILD_BEACON

/* beacon.dll needs no per-process or per-thread initialization. */
BOOL WINAPI _DllMainCRTStartup(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reason;
    (void)reserved;
    return TRUE;
}

#else

typedef struct {
    int new_mode;
} pe_startup_info;

extern int __cdecl msvcrt_wgetmainargs(int *, wchar_t ***, wchar_t ***, int,
                                       pe_startup_info *)
    PE_MSVCRT(__wgetmainargs);

int wmain(int argc, WCHAR **argv);

/*
 * Entry point for standalone runs. Beacon calls the exported go function
 * directly and never runs this.
 */
void wmainCRTStartup(void)
{
    int argc = 0;
    wchar_t **argv = NULL;
    wchar_t **environment = NULL;
    pe_startup_info startup = { 0 };

    if (msvcrt_wgetmainargs(&argc, &argv, &environment, 0, &startup) < 0) {
        ExitProcess(255);
    }
    ExitProcess((UINT)wmain(argc, argv));
}

#endif
