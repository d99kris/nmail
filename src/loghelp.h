// loghelp.h
//
// Copyright (c) 2019-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstring>
#include <map>
#include <sstream>

#include "cxx-prettyprint/prettyprint.hpp"

#include "log.h"

#define __FILENAME__ (strrchr("/" __FILE__, '/') + 1)

#define LOG_TRACE(...) Log::Trace(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) Log::Debug(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Log::Info(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) Log::Warning(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Log::Error(__FILENAME__, __LINE__, __VA_ARGS__)

#define LOG_DUMP(STR) Log::Dump(STR)

#define LOG_TRACE_FUNC(ARGS) do { if (!Log::GetTraceEnabled()) { break; } \
                                  const std::string& str = ARGS; \
                                  Log::Trace(__FILENAME__, __LINE__, "%s(%s)", \
                                             __FUNCTION__, str.c_str()); } while (0)

#define LOG_DEBUG_FUNC(ARGS) do { if (!Log::GetDebugEnabled()) { break; } \
                                  const std::string& str = ARGS; \
                                  Log::Debug(__FILENAME__, __LINE__, "%s(%s)", \
                                             __FUNCTION__, str.c_str()); } while (0)

#define LOG_DEBUG_VAR(MSG, VAR) do { if (!Log::GetDebugEnabled()) { break; } \
                                     const std::string& str = LogHelp::PrettyPrint(VAR); \
                                     LOG_DEBUG(MSG " %s", str.c_str()); } while (0)

// logs error on failure, nothing on success
#define LOG_IF_NONZERO(EXPR) LogHelp::LogIfNotEqual(EXPR, 0, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_BADFD(EXPR) LogHelp::LogIfEqual(EXPR, -1, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_NULL(EXPR) LogHelp::LogIfEqual(EXPR, NULL, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_NOT_EQUAL(EXPR, EXPECT) LogHelp::LogIfNotEqual(EXPR, EXPECT, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_FALSE(EXPR) LogHelp::LogIfEqual(EXPR, false, #EXPR, __FILENAME__, __LINE__)

// logs error on failure, logs debug on success
#define LOG_IF_IMAP_ERR(EXPR) LogHelp::LogImap(EXPR, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_IMAP_LOGOUT_ERR(EXPR) LogHelp::LogImapLogout(EXPR, #EXPR, __FILENAME__, __LINE__)
#define LOG_IF_SMTP_ERR(EXPR) LogHelp::LogSmtp(EXPR, #EXPR, __FILENAME__, __LINE__)

#define LOG_DURATION() LogDuration logDuration(__FUNCTION__, __FILENAME__, __LINE__)

#define LOG_LATENCY_START(METRIC) LogLatency::Start(METRIC)
#define LOG_LATENCY_END(METRIC) LogLatency::End(__FILENAME__, __LINE__, METRIC)

class LogHelp
{
public:
  static std::string ImapErrToStr(int p_ImapErr);
  static std::string SmtpErrToStr(int p_SmtpErr);

  static int LogImap(int p_Rv, const char* p_Expr, const char* p_File, int p_Line);
  static int LogImapLogout(int p_Rv, const char* p_Expr, const char* p_File, int p_Line);
  static int LogSmtp(int p_Rv, const char* p_Expr, const char* p_File, int p_Line);

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
                                    const RemainingArgs& ... p_RemainingArgs)
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
std::string STR(const Args& ... p_Args)
{
  std::stringstream sstream;
  LogHelp::PrettyPrintArgsHelper(sstream, p_Args...);
  return sstream.str();
}

class LogDuration
{
public:
  LogDuration(const char* p_Func, const char* p_File, int p_Line)
    : m_Func(p_Func)
    , m_File(p_File)
    , m_Line(p_Line)
  {
    m_Start = std::chrono::high_resolution_clock::now();
  }

  ~LogDuration()
  {
    if (Log::GetTraceEnabled())
    {
      const std::chrono::high_resolution_clock::time_point stop =
        std::chrono::high_resolution_clock::now();
      const std::chrono::duration<double> duration =
        std::chrono::duration_cast<std::chrono::duration<double>>(stop - m_Start);
      long long durationUs = static_cast<long long>(round(duration.count() * 1000000.0));
      Log::Trace(m_File, m_Line, "%s() duration %lld us", m_Func, durationUs);
    }
  }

private:
  std::chrono::high_resolution_clock::time_point m_Start;
  const char* m_Func = nullptr;
  const char* m_File = nullptr;
  int m_Line = 0;
};

enum LatencyMetric
{
  LatencyKeyPress = 0,
};

class LogLatency
{
public:
  static void Start(LatencyMetric p_LatencyMetric)
  {
    if (Log::GetTraceEnabled())
    {
      std::chrono::high_resolution_clock::time_point startTime = std::chrono::high_resolution_clock::now();
      std::unique_lock<std::mutex> lock(m_Mutex);
      m_StartTimes[p_LatencyMetric] = startTime;
    }
  }

  static void End(const char* p_File, int p_Line, LatencyMetric p_LatencyMetric)
  {
    if (Log::GetTraceEnabled())
    {
      std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
      std::unique_lock<std::mutex> lock(m_Mutex);
      auto it = m_StartTimes.find(p_LatencyMetric);
      if (it != m_StartTimes.end())
      {
        std::chrono::high_resolution_clock::time_point startTime = it->second;
        // uncomment to allow single measurement: m_StartTimes.erase(it);
        lock.unlock();

        const std::chrono::duration<double> duration =
          std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
        const long long durationMs = static_cast<long long>(round(duration.count() * 1000.0));

        static std::map<LatencyMetric, std::string> s_MetricDescriptions =
        {
          { LatencyKeyPress, "key to ui draw" },
        };

        const std::string description = s_MetricDescriptions[p_LatencyMetric];

        Log::Trace(p_File, p_Line, "latency %s %lld ms", description.c_str(), durationMs);
      }
    }
  }

private:
  static std::mutex m_Mutex;
  static std::map<LatencyMetric, std::chrono::high_resolution_clock::time_point> m_StartTimes;
};
