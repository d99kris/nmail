// apputil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <csignal>
#include <map>
#include <mutex>
#include <string>

#define THREAD_REGISTER() ThreadRegister threadRegister(__PRETTY_FUNCTION__)
#define nm_assert(cond) \
        do { if (!(cond)) { LOG_ERROR("Assertion failed: %s", #cond); \
                            AppUtil::AssertionFailed(); } } while (0)

class ThreadRegister
{
public:
  ThreadRegister(const std::string& p_Name)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Threads.insert({ pthread_self(), p_Name });
  }

  ~ThreadRegister()
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Threads.erase(pthread_self());
  }

  static std::string GetName()
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Threads.find(pthread_self());
    return (it != m_Threads.end()) ? it->second : "";
  }

  static void SignalThreads(int p_Sig)
  {
    pthread_t self = pthread_self();
    for (auto it = m_Threads.begin(); it != m_Threads.end(); ++it)
    {
      if (it->first != self)
      {
        pthread_kill(it->first, p_Sig);
      }
    }
  }

private:
  static std::mutex m_Mutex;
  static std::map<pthread_t, std::string> m_Threads;
};

class AppUtil
{
public:
  static std::string GetApplicationDir();
  static void SetApplicationDir(const std::string& p_Path);
  static void InitAppSignalHandlers();
  static void InitUiSignalHandlers();
  static void CleanupUiSignalHandlers();
  static void SignalCrashHandler(int p_Signal, siginfo_t* p_SigInfo, void* p_Context);
  static void SignalTerminateHandler(int p_Signal);
  static bool IsTerminateRequested();
  static int GetTerminateSignal();
  static int GetCallstack(void** p_Callstack, int p_MaxSize, void* p_Context);
  static void InitStdErrRedirect(const std::string& p_Path);
  static void CleanupStdErrRedirect();
  static std::map<int, std::string> GetCrashingSignals();
  static std::map<int, std::string> GetTerminatingSignals();
  static std::map<int, std::string> GetIgnoredSignals();
  static std::string GetSigName(int p_Signal);
  static void InitCoredump();
  static void CheckEnvironment();
  static void InitLocale();
  static std::string GetDefaultApplicationDir();
  static void SetReadOnly(bool p_ReadOnly);
  static bool GetReadOnly();
  static void SetAssertAbort(bool p_AssertAbort);
  static void AssertionFailed();

private:
  static std::string m_ApplicationDir;
  static int m_OrgStdErr;
  static int m_NewStdErr;
  static bool m_ReadOnly;
  static volatile sig_atomic_t m_TerminateSignal;
  static bool m_AssertAbort;
};
