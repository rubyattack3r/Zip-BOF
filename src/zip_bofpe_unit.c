/*
 * Builds the zip BOF-PE as one translation unit, which lets the compiler drop
 * code that the executable never calls.
 */
#include "miniz_static.h"

#include "pe_runtime.c"
#include "stack_probe.c"
#include "zip_path.c"
#include "zip_archive.c"
#include "zip_bofpe.c"
#include "miniz.c"
#include "miniz_tdef.c"
#include "miniz_tinfl.c"
#include "miniz_zip.c"
