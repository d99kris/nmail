// log.h
//
// Copyright (c) 2019-2024 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <mutex>
#include <string>

class Log
{
public:
  static const int INFO_LEVEL = 0;
  static const int DEBUG_LEVEL = 1;
  static const int TRACE_LEVEL = 2;

  static void SetPath(const std::string& p_Path);
  static void Cleanup(bool p_IsLogdumpEnabled);
  static void SetVerboseLevel(int p_Level);
  static inline int GetVerboseLevel() { return m_VerboseLevel; }
  static inline bool GetTraceEnabled() { return m_VerboseLevel >= TRACE_LEVEL; }
  static inline bool GetDebugEnabled() { return m_VerboseLevel >= DEBUG_LEVEL; }

  static void Trace(const char* p_Filename, int p_LineNo, const char* p_Format, ...);
  static void Debug(const char* p_Filename, int p_LineNo, const char* p_Format, ...);
  static void Info(const char* p_Filename, int p_LineNo, const char* p_Format, ...);
  static void Warning(const char* p_Filename, int p_LineNo, const char* p_Format, ...);
  static void Error(const char* p_Filename, int p_LineNo, const char* p_Format, ...);

  static void Dump(const char* p_Str);
  static void Callstack(void* const* p_Callstack, int p_Size, const char* p_LogMsg);

private:
  static void Write(const char* p_Filename, int p_LineNo, const char* p_Level, const char* p_Format, va_list p_VaList);

private:
  static std::string m_Path;
  static int m_VerboseLevel;
  static std::mutex m_Mutex;
  static int m_LogFd;
  static bool m_HadWarnErr;
};
