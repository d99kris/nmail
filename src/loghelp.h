// loghelp.h
//
// Copyright (c) 2019 Kristofer Berggren
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

#define LOG_DEBUG(EXPR, ...) Log::Debug(EXPR "  (%s:%d)", ##__VA_ARGS__, __FILENAME__, __LINE__)
#define LOG_INFO(EXPR, ...) Log::Info(EXPR "  (%s:%d)", ##__VA_ARGS__, __FILENAME__, __LINE__)
#define LOG_WARNING(EXPR, ...) Log::Warning(EXPR "  (%s:%d)", ##__VA_ARGS__, __FILENAME__, __LINE__)
#define LOG_ERROR(EXPR, ...) Log::Error(EXPR "  (%s:%d)", ##__VA_ARGS__, __FILENAME__, __LINE__)
#define LOG_DUMP(STR) Log::Dump(STR)
#define LOG_DEBUG_VAR(MSG, VAR) do { const std::string& str = PrettyPrint(VAR); LOG_DEBUG(MSG " %s", str.c_str()); } while (0)

#define LOG_IF_NULL(EXPR) LogIfEqual(EXPR, NULL, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_SMTP_ERR(EXPR) LogIfNotEqual(EXPR, MAILSMTP_NO_ERROR, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_IMAP_ERR(EXPR) LogImapHelper(EXPR, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_IMAP_LOGOUT_ERR(EXPR) LogImapLogoutHelper(EXPR, #EXPR, __FILENAME__, __LINE__)

static inline int LogImapHelper(int p_Rv, const char* p_Expr, const char* p_File, int p_Line) 
{
  if (p_Rv > MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
  {
    Log::Error("%s = %d  (%s:%d)", p_Expr, p_Rv, p_File, p_Line);
  }

  return p_Rv;
}

static inline int LogImapLogoutHelper(int p_Rv, const char* p_Expr, const char* p_File,
                                      int p_Line) 
{
  if ((p_Rv > MAILIMAP_NO_ERROR_NON_AUTHENTICATED) && (p_Rv != MAILIMAP_ERROR_STREAM))
  {
    Log::Error("%s = %d  (%s:%d)", p_Expr, p_Rv, p_File, p_Line);
  }

  return p_Rv;
}

template <typename T>
struct identity { typedef T type; };

template <typename T>
static inline T LogIfNotEqual(T p_Rv, typename identity<T>::type p_Expect, const char* p_Expr,
                              const char* p_File, int p_Line) 
{
  if (p_Rv != p_Expect)
  {
    Log::Error("%s = %d  (%s:%d)", p_Expr, p_Rv, p_File, p_Line);
  }

  return p_Rv;
}

template <typename T>
static inline T LogIfEqual(T p_Rv, typename identity<T>::type p_Expect, const char* p_Expr,
                           const char* p_File, int p_Line) 
{
  if (p_Rv == p_Expect)
  {
    Log::Error("%s = %d  (%s:%d)", p_Expr, p_Rv, p_File, p_Line);
  }

  return p_Rv;
}

template <typename T>
static inline std::string PrettyPrint(const T& p_Container)
{
  std::stringstream sstream;
  sstream << p_Container;
  return sstream.str();
}
