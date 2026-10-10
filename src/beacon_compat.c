#include "beacon.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WCHAR *utf8_to_wide(const char *value)
{
    WCHAR *converted;
    int required;

    required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
                                   NULL, 0);
    if (required <= 0) {
        return NULL;
    }
    converted = (WCHAR *)malloc((size_t)required * sizeof(WCHAR));
    if (converted == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
                            converted, required) <= 0) {
        free(converted);
        return NULL;
    }
    return converted;
}

static uint32_t read_be32(const unsigned char *input)
{
    return ((uint32_t)input[0] << 24U) |
           ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) |
           (uint32_t)input[3];
}

BEACON_API void BeaconDataParse(datap *parser, char *buffer, int size)
{
    if (parser == NULL) {
        return;
    }

    parser->original = buffer;
    parser->buffer = buffer;
    parser->length = size > 0 ? size : 0;
    parser->size = parser->length;
}

BEACON_API int BeaconDataInt(datap *parser)
{
    uint32_t value;

    if (parser == NULL || parser->buffer == NULL || parser->length < 4) {
        return 0;
    }

    value = read_be32((const unsigned char *)parser->buffer);
    parser->buffer += 4;
    parser->length -= 4;
    return (int)value;
}

BEACON_API char *BeaconDataExtract(datap *parser, int *size)
{
    uint32_t length;
    char *value;

    if (size != NULL) {
        *size = 0;
    }
    if (parser == NULL || parser->buffer == NULL || parser->length < 4) {
        return NULL;
    }

    length = read_be32((const unsigned char *)parser->buffer);
    parser->buffer += 4;
    parser->length -= 4;

    if (length > (uint32_t)parser->length) {
        parser->buffer += parser->length;
        parser->length = 0;
        return NULL;
    }

    value = parser->buffer;
    parser->buffer += length;
    parser->length -= (int)length;
    if (size != NULL) {
        *size = (int)length;
    }
    return value;
}

BEACON_API void BeaconPrintf(int type, const char *format, ...)
{
    va_list arguments;
    DWORD stream =
        (type == CALLBACK_ERROR || type == CALLBACK_ERROR_UTF8)
            ? stderr
            : stdout;

    va_start(arguments, format);
    (void)vfprintf(stream, format, arguments);
    va_end(arguments);
}

BEACON_API BOOL BeaconDownload(const char *filename, const char *buffer,
                               unsigned int length)
{
    HANDLE output;
    WCHAR *wide_filename;
    DWORD written = 0;
    BOOL ok;

    if (filename == NULL || filename[0] == '\0' ||
        (buffer == NULL && length != 0U)) {
        return FALSE;
    }

    wide_filename = utf8_to_wide(filename);
    if (wide_filename == NULL) {
        return FALSE;
    }
    output = CreateFileW(wide_filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        free(wide_filename);
        return FALSE;
    }
    ok = length == 0U ||
         (WriteFile(output, buffer, length, &written, NULL) &&
          written == length);
    if (!CloseHandle(output) || !ok) {
        (void)DeleteFileW(wide_filename);
        free(wide_filename);
        return FALSE;
    }
    free(wide_filename);
    return TRUE;
}
