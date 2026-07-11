// version.cpp
//
// Copyright (c) 2022-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "version.h"

#include "buildinfo.h"

#define NMAIL_VERSION "5.14.10"

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

std::string Version::GetAppName(bool p_WithVersion, bool p_WithBranch /*= false*/)
{
  std::string name = "nmail";
  if (p_WithVersion)
  {
    name += " " + GetAppVersion();
  }

#if defined(NMAIL_BUILD_GIT_BRANCH)
  if (p_WithBranch)
  {
    const std::string branch = NMAIL_BUILD_GIT_BRANCH;
    if (!branch.empty() && (branch != "master"))
    {
      name += " " + branch;
    }
  }
#else
  (void)p_WithBranch;
#endif

  return name;
}

std::string Version::GetAppVersion()
{
  static std::string version = NMAIL_VERSION;
  return version;
}

// Build origin/sha are read from the generated buildinfo.h here (rather than in
// util.cpp) so the volatile buildinfo.h only ever recompiles this trivial file,
// not the heavy util.cpp. See buildinfo.h.in / CMakeLists.txt.
std::string Version::GetBuildOrigin()
{
#if defined(NMAIL_BUILD_ORIGIN)
  return NMAIL_BUILD_ORIGIN;
#else
  return "local";
#endif
}

std::string Version::GetBuildGitSha()
{
#if defined(NMAIL_BUILD_GIT_SHA)
  return NMAIL_BUILD_GIT_SHA;
#else
  return "unknown";
#endif
}
