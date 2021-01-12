// loghelp.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstring>
#include <sstream>

#include <libetpan/libetpan.h>

#include "cxx-prettyprint/prettyprint.hpp"

#include "log.h"

#define __FILENAME__ (strrchr("/" __FILE__, '/') + 1)

#define LOG_TRACE(...) Log::Trace(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) Log::Debug(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Log::Info(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) Log::Warning(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Log::Error(__FILENAME__, __LINE__, __VA_ARGS__)

#define LOG_DUMP(STR) Log::Dump(STR)
#define LOG_TRACE_FUNC(ARGS) do { if (!Log::GetTraceEnabled())break; \
                                  const std::string& str = ARGS; \
                                  Log::Trace(__FILENAME__, __LINE__, "%s(%s)", __FUNCTION__, str.c_str()); \
} while(0)
#define LOG_DEBUG_FUNC(ARGS) do { if (!Log::GetDebugEnabled())break; \
                                  const std::string& str = ARGS; \
                                  Log::Debug(__FILENAME__, __LINE__, "%s(%s)", __FUNCTION__, str.c_str()); \
} while(0)
#define LOG_DEBUG_VAR(MSG, VAR) do { if (!Log::GetDebugEnabled())break; \
                                     const std::string& str = LogHelp::PrettyPrint(VAR); \
                                     LOG_DEBUG(MSG " %s", str.c_str()); \
} while (0)

#define LOG_IF_NONZERO(EXPR) LogHelp::LogIfNotEqual(EXPR, 0, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_BADFD(EXPR) LogHelp::LogIfEqual(EXPR, -1, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_NULL(EXPR) LogHelp::LogIfEqual(EXPR, NULL, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_IMAP_ERR(EXPR) LogHelp::LogImap(EXPR, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_IMAP_LOGOUT_ERR(EXPR) LogHelp::LogImapLogout(EXPR, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_SMTP_ERR(EXPR) LogHelp::LogSmtp(EXPR, #EXPR, __FILENAME__, __LINE__)

class LogHelp
{
public:
  static std::string ImapErrToStr(int p_ImapErr);
  static std::string SmtpErrToStr(int p_SmtpErr);

  static inline int LogImap(int p_Rv, const char* p_Expr, const char* p_File, int p_Line)
  {
    if (p_Rv > MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
    {
      Log::Error(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
    }
    else if (Log::GetDebugEnabled())
    {
      Log::Debug(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
    }

    return p_Rv;
  }

  static inline int LogImapLogout(int p_Rv, const char* p_Expr, const char* p_File, int p_Line)
  {
    if ((p_Rv > MAILIMAP_NO_ERROR_NON_AUTHENTICATED) && (p_Rv != MAILIMAP_ERROR_STREAM))
    {
      Log::Error(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
    }
    else if (Log::GetDebugEnabled())
    {
      Log::Debug(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
    }

    return p_Rv;
  }

  static inline int LogSmtp(int p_Rv, const char* p_Expr, const char* p_File, int p_Line)
  {
    if (p_Rv != MAILSMTP_NO_ERROR)
    {
      Log::Error(p_File, p_Line, "%s = %s", p_Expr, SmtpErrToStr(p_Rv).c_str());
    }
    else if (Log::GetDebugEnabled())
    {
      Log::Debug(p_File, p_Line, "%s = %s", p_Expr, SmtpErrToStr(p_Rv).c_str());
    }

    return p_Rv;
  }

  template<typename T>
  struct identity { typedef T type; };

  template<typename T>
  static inline T LogIfNotEqual(T p_Rv, typename identity<T>::type p_Expect, const char* p_Expr,
                                const char* p_File, int p_Line)
  {
    if (p_Rv != p_Expect)
    {
      Log::Error(p_File, p_Line, "%s = 0x%x", p_Expr, p_Rv);
    }
    else if (Log::GetDebugEnabled())
    {
      Log::Debug(p_File, p_Line, "%s = 0x%x", p_Expr, p_Rv);
    }

    return p_Rv;
  }

  template<typename T>
  static inline T LogIfEqual(T p_Rv, typename identity<T>::type p_Expect, const char* p_Expr,
                             const char* p_File, int p_Line)
  {
    if (p_Rv == p_Expect)
    {
      Log::Error(p_File, p_Line, "%s = 0x%x", p_Expr, p_Rv);
    }
    else if (Log::GetDebugEnabled())
    {
      Log::Debug(p_File, p_Line, "%s = 0x%x", p_Expr, p_Rv);
    }

    return p_Rv;
  }

  template<typename T>
  static inline std::string PrettyPrint(const T& p_Container)
  {
    std::stringstream sstream;
    sstream << p_Container;
    return sstream.str();
  }

  static void PrettyPrintArgsHelper(std::stringstream& /*p_Sstream*/);

  template<typename FirstArg, typename... RemainingArgs>
  static void PrettyPrintArgsHelper(std::stringstream& p_Sstream, const FirstArg& p_FirstArg,
                                    const RemainingArgs&... p_RemainingArgs)
  {
    if (!p_Sstream.str().empty())
    {
      p_Sstream << ", ";
    }

    p_Sstream << p_FirstArg;

    PrettyPrintArgsHelper(p_Sstream, p_RemainingArgs...);
  }
};

template<typename... Args>
std::string STR(const Args&... p_Args)
{
  std::stringstream sstream;
  LogHelp::PrettyPrintArgsHelper(sstream, p_Args...);
  return sstream.str();
}
