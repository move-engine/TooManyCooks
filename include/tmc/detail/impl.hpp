// Copyright (c) 2023-2026 Logan McDougall
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

// When TMC_STANDALONE_COMPILATION is defined, the user must #define TMC_IMPL
// and #include the necessary headers (easiest to #include
// "tmc/all_headers.hpp") in exactly one translation unit. This provides
// definitions for non-template functions in that translation unit.

// When TMC_SHARED_LIBRARY is defined, this also implies
// TMC_STANDALONE_COMPILATION. The user must provide the impl translation unit,
// which owns the library's process-wide globals and thread-local state. On
// Windows it also exports the necessary symbols; consumers import them.
// TMC_WINDOWS_DLL is retained as a backwards-compatible spelling.

// When neither are defined, the library is header-only, and all symbols are
// declared "inline".

#if defined(TMC_WINDOWS_DLL) && !defined(TMC_SHARED_LIBRARY)
#define TMC_SHARED_LIBRARY
#endif

#ifdef TMC_SHARED_LIBRARY
#ifndef TMC_STANDALONE_COMPILATION
#define TMC_STANDALONE_COMPILATION
#endif
#endif

#ifdef TMC_STANDALONE_COMPILATION
#if defined(TMC_SHARED_LIBRARY) && defined(_WIN32)
#ifdef TMC_IMPL
#define TMC_DECL __declspec(dllexport)
#else
#define TMC_DECL __declspec(dllimport)
#endif
#elif defined(TMC_SHARED_LIBRARY) &&                                      \
  (defined(__GNUC__) || defined(__clang__))
#define TMC_DECL __attribute__((visibility("default")))
#else
#define TMC_DECL
#endif
#else
#define TMC_DECL inline
#endif
