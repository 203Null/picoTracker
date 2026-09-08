/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Emscripten/Clang does not accept libc strlen in non-type template arguments,
 * while the embedded compilers accept these existing PicoTracker constants.
 */

#ifndef PICOTRACKER_WASM_COMPAT_H
#define PICOTRACKER_WASM_COMPAT_H

#include "Externals/etl/include/etl/string.h"
#include "Externals/etl/include/etl/string_utilities.h"
#include <string.h>

#define strlen(value) __builtin_strlen(value)

#endif
