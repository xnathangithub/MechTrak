#ifndef IMGUI_PCH_H
#define IMGUI_PCH_H

// Minimal precompiled header for files inside the IMGUI folder.
// Keeps compile-unit-local includes that many imgui .cpp files expect.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cctype>
#include <cfloat>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <climits>
#include <limits>

// Forward to project's root pch if present (optional).
#if defined(__has_include)
#  if __has_include("../pch.h")
#    include "../pch.h"
#  endif
#endif

#endif // IMGUI_PCH_H