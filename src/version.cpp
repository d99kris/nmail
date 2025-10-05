// version.cpp
//
// Copyright (c) 2022-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "version.h"

#define NMAIL_VERSION "5.8.3"

std::string Version::GetBuildOs()
{
#if defined(_WIN32)
  return "Windows";
#elif defined(__APPLE__)
  return "macOS";
#elif defined(__linux__)
  return "Linux";
#elif defined(BSD)
  return "BSD";
#else
  return "Unknown OS";
#endif
}

std::string Version::GetCompiler()
{
#if defined(_MSC_VER)
  return "msvc-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
  return "clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__)
         + "." + std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
  return "gcc-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__)
         + "." + std::to_string(__GNUC_PATCHLEVEL__);
#else
  return "Unknown Compiler";
#endif
}

std::string Version::GetAppName(bool p_WithVersion)
{
  return std::string("nmail") + (p_WithVersion ? " " NMAIL_VERSION : "");
}
