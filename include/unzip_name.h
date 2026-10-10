#ifndef UNZIP_NAME_H
#define UNZIP_NAME_H

#include <stddef.h>

/* Entry names are UTF-8 with '/' separators, never target filesystem paths. */
int unzip_name_validate(const char *name, size_t length, int is_directory);

#endif
