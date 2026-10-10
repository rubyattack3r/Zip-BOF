#ifndef ZIP_BEACON_H
#define ZIP_BEACON_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Native BOFs import the Beacon API like any DLL function: Beacon's loader
 * resolves the __imp_ symbols that __declspec(dllimport) references. */
#ifdef ZIP_BOF
#define BEACON_API __declspec(dllimport)
#elif defined(BUILD_BEACON)
#define BEACON_API __declspec(dllexport)
#else
#define BEACON_API __declspec(dllimport)
#endif

#define CALLBACK_OUTPUT       0x00
#define CALLBACK_ERROR        0x0d
#define CALLBACK_ERROR_UTF8   0x1f
#define CALLBACK_OUTPUT_UTF8  0x20

typedef struct datap {
    char *original;
    char *buffer;
    int length;
    int size;
} datap;

BEACON_API void BeaconDataParse(datap *parser, char *buffer, int size);
BEACON_API int BeaconDataInt(datap *parser);
BEACON_API char *BeaconDataExtract(datap *parser, int *size);
BEACON_API void BeaconPrintf(int type, const char *format, ...);
BEACON_API BOOL BeaconDownload(const char *filename, const char *buffer,
                               unsigned int length);

/* BOF-PE entry points do not need executable startup initialization. */
#define BEACON_INIT() do { } while (0)

#ifdef __cplusplus
}
#endif

#endif
