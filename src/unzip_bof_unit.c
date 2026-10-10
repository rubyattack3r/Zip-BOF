/*
 * Builds the native unzip BOF as one translation unit. Zig cannot merge COFF
 * objects into one relocatable object, and a single unit lets the compiler
 * drop code that the BOF never calls.
 */
#include "miniz_static.h"

#include "bof_runtime.c"
#include "stack_probe.c"
#include "zip_path.c"
#include "unzip_name.c"
#include "unzip_archive.c"
#include "unzip_bof.c"
#include "miniz.c"
#include "miniz_tdef.c"
#include "miniz_tinfl.c"
#include "miniz_zip.c"
