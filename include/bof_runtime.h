#ifndef ZIP_BOF_RUNTIME_H
#define ZIP_BOF_RUNTIME_H

#ifdef ZIP_BOF

#define WIN32_LEAN_AND_MEAN
/* Match miniz_zip.c so its later definition is not a redefinition. */
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <windows.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* BOF imports use the MODULE$symbol convention. __declspec(dllimport) makes
 * each call go through an __imp_MODULE$symbol pointer, which Beacon's loader
 * resolves; plain declarations would leave undefined symbols it cannot. */
extern __declspec(dllimport) HANDLE WINAPI KERNEL32$GetProcessHeap(void);
extern __declspec(dllimport) LPVOID WINAPI KERNEL32$HeapAlloc(HANDLE, DWORD, SIZE_T);
extern __declspec(dllimport) LPVOID WINAPI KERNEL32$HeapReAlloc(HANDLE, DWORD, LPVOID, SIZE_T);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$HeapFree(HANDLE, DWORD, LPVOID);
extern __declspec(dllimport) DWORD WINAPI KERNEL32$GetFullPathNameW(LPCWSTR, DWORD, LPWSTR, LPWSTR *);
extern __declspec(dllimport) DWORD WINAPI KERNEL32$GetFileAttributesW(LPCWSTR);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$GetFileAttributesExW(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
extern __declspec(dllimport) int WINAPI KERNEL32$CompareStringOrdinal(LPCWCH, int, LPCWCH, int, BOOL);
extern __declspec(dllimport) int WINAPI KERNEL32$WideCharToMultiByte(UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL);
extern __declspec(dllimport) HANDLE WINAPI KERNEL32$FindFirstFileW(LPCWSTR, LPWIN32_FIND_DATAW);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$FindNextFileW(HANDLE, LPWIN32_FIND_DATAW);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$FindClose(HANDLE);
extern __declspec(dllimport) DWORD WINAPI KERNEL32$GetLastError(void);
extern __declspec(dllimport) DWORD WINAPI KERNEL32$GetTickCount(void);
extern __declspec(dllimport) DWORD WINAPI KERNEL32$GetCurrentProcessId(void);
extern __declspec(dllimport) HANDLE WINAPI KERNEL32$CreateFileW(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$CloseHandle(HANDLE);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$FlushFileBuffers(HANDLE);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$DeleteFileW(LPCWSTR);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$MoveFileExW(LPCWSTR, LPCWSTR, DWORD);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$CreateDirectoryW(LPCWSTR, LPSECURITY_ATTRIBUTES);
extern __declspec(dllimport) BOOL WINAPI KERNEL32$SetFilePointerEx(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);
extern __declspec(dllimport) int WINAPI KERNEL32$MultiByteToWideChar(UINT, DWORD, LPCCH, int, LPWSTR, int);

#define GetProcessHeap KERNEL32$GetProcessHeap
#define HeapAlloc KERNEL32$HeapAlloc
#define HeapReAlloc KERNEL32$HeapReAlloc
#define HeapFree KERNEL32$HeapFree
#define GetFullPathNameW KERNEL32$GetFullPathNameW
#define GetFileAttributesW KERNEL32$GetFileAttributesW
#define GetFileAttributesExW KERNEL32$GetFileAttributesExW
#define CompareStringOrdinal KERNEL32$CompareStringOrdinal
#define WideCharToMultiByte KERNEL32$WideCharToMultiByte
#define FindFirstFileW KERNEL32$FindFirstFileW
#define FindNextFileW KERNEL32$FindNextFileW
#define FindClose KERNEL32$FindClose
#define GetLastError KERNEL32$GetLastError
#define GetTickCount KERNEL32$GetTickCount
#define GetCurrentProcessId KERNEL32$GetCurrentProcessId
#define CreateFileW KERNEL32$CreateFileW
#define ReadFile KERNEL32$ReadFile
#define WriteFile KERNEL32$WriteFile
#define CloseHandle KERNEL32$CloseHandle
#define FlushFileBuffers KERNEL32$FlushFileBuffers
#define DeleteFileW KERNEL32$DeleteFileW
#define MoveFileExW KERNEL32$MoveFileExW
#define CreateDirectoryW KERNEL32$CreateDirectoryW
#define SetFilePointerEx KERNEL32$SetFilePointerEx
#define MultiByteToWideChar KERNEL32$MultiByteToWideChar

void *bof_malloc(size_t size);
void *bof_realloc(void *pointer, size_t size);
void bof_free(void *pointer);
int bof_vsnprintf(char *buffer, size_t capacity, const char *format, va_list args);
int bof_snprintf(char *buffer, size_t capacity, const char *format, ...);
int bof_snwprintf(WCHAR *buffer, size_t capacity, const WCHAR *format, ...);

#define malloc bof_malloc
#define realloc bof_realloc
#define free bof_free
#define snprintf bof_snprintf
#define vsnprintf bof_vsnprintf
#define _snwprintf bof_snwprintf

extern __declspec(dllimport) void * __cdecl MSVCRT$memcpy(void *, const void *, size_t);
extern __declspec(dllimport) void * __cdecl MSVCRT$memmove(void *, const void *, size_t);
extern __declspec(dllimport) void * __cdecl MSVCRT$memset(void *, int, size_t);
extern __declspec(dllimport) int __cdecl MSVCRT$memcmp(const void *, const void *, size_t);
extern __declspec(dllimport) size_t __cdecl MSVCRT$strlen(const char *);
extern __declspec(dllimport) int __cdecl MSVCRT$strcmp(const char *, const char *);
extern __declspec(dllimport) int __cdecl MSVCRT$strncmp(const char *, const char *, size_t);
extern __declspec(dllimport) size_t __cdecl MSVCRT$wcslen(const wchar_t *);
extern __declspec(dllimport) int __cdecl MSVCRT$wcscmp(const wchar_t *, const wchar_t *);
extern __declspec(dllimport) int __cdecl MSVCRT$wcsncmp(const wchar_t *, const wchar_t *, size_t);

#define memcpy MSVCRT$memcpy
#define memmove MSVCRT$memmove
#define memset MSVCRT$memset
#define memcmp MSVCRT$memcmp
#define strlen MSVCRT$strlen
#define strcmp MSVCRT$strcmp
#define strncmp MSVCRT$strncmp
#define wcslen MSVCRT$wcslen
#define wcscmp MSVCRT$wcscmp
#define wcsncmp MSVCRT$wcsncmp

#endif
#endif
