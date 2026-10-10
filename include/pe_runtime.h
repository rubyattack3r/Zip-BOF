#ifndef ZIP_PE_RUNTIME_H
#define ZIP_PE_RUNTIME_H

/*
 * BOF-PEs link without a C runtime and import the C functions they use from
 * msvcrt.dll. The toolchain headers target the UCRT, whose stdio functions
 * do not exist in msvcrt.dll, so formatted output and printing are redirected
 * to the wrappers below. Other C functions keep their names and resolve
 * through the msvcrt.dll import library.
 */

#define WIN32_LEAN_AND_MEAN
/* Match miniz_zip.c so its later definition is not a redefinition. */
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int pe_vsnprintf(char *buffer, size_t capacity, const char *format, va_list args);
int pe_snprintf(char *buffer, size_t capacity, const char *format, ...);
int pe_snwprintf(WCHAR *buffer, size_t capacity, const WCHAR *format, ...);
int pe_vfprintf(DWORD std_handle, const char *format, va_list args);
int pe_fprintf(DWORD std_handle, const char *format, ...);

#undef stdout
#undef stderr
#define stdout STD_OUTPUT_HANDLE
#define stderr STD_ERROR_HANDLE

#define snprintf pe_snprintf
#define vsnprintf pe_vsnprintf
#define _snwprintf pe_snwprintf
#define vfprintf pe_vfprintf
#define fprintf pe_fprintf
#define printf(...) pe_fprintf(STD_OUTPUT_HANDLE, __VA_ARGS__)

#endif
