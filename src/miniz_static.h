/*
 * Included first by the single-translation-unit builds. Declaring the miniz
 * API static lets the compiler drop functions that a binary never calls.
 *
 * miniz_common.h declares its allocators extern, so it is included before the
 * switch. Definitions without a storage class then inherit internal linkage
 * from the static declarations in the remaining headers.
 */
#ifndef ZIP_MINIZ_STATIC_H
#define ZIP_MINIZ_STATIC_H

#include "miniz_common.h"

#undef MINIZ_EXPORT
#define MINIZ_EXPORT static
#include "miniz.h"
#undef MINIZ_EXPORT
#define MINIZ_EXPORT

#endif
