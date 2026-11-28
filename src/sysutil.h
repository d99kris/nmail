// sysutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

#include <sys/types.h>

#define UNUSED(x) SysUtil::Unused(x)

class SysUtil
{
public:
  static std::string ExtractEmbeddedScript(const std::string& p_Name,
                                           const unsigned char* p_Data, size_t p_Size);
  static std::string RunCommand(const std::string& p_Cmd);
  static void DetectCommandNotPresent(const std::string& p_Cmd);
  static std::string GetSystemOs();
  static std::string GetLinkedLibs(const std::string& p_Prog);
  static std::string GetSelfPath();
  static std::string GetLibetpanVersion();
  static std::string GetUname();
  static std::string GetSQLiteVersion();
  static std::string GetBuildInfo();
  static std::string GetCompiler(bool p_Verbose);
  static std::string GetOsArch(bool p_Verbose);
  static bool IsIpAddress(const std::string& p_Str);
  static bool IsProcessRunning(pid_t p_Pid);
  static bool IsSelfProcess(pid_t p_Pid);
  static int System(const std::string& p_Cmd);
  static std::string GetPass();

  template<typename T>
  static inline void Unused(const T& p_Arg)
  {
    (void)p_Arg;
  }
};
