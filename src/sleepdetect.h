// sleepdetect.h
//
// Copyright (c) 2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <functional>
#include <condition_variable>
#include <mutex>
#include <thread>

class SleepDetect
{
public:
  SleepDetect(const std::function<void()>& p_OnWakeUp, int p_MinSleepSec);
  ~SleepDetect();

  void Process();

private:
  std::function<void()> m_OnWakeUp;
  int m_MinSleepSec = 0;
  bool m_Running = false;
  std::thread m_Thread;
  std::mutex m_Mutex;
  std::condition_variable m_CondVar;
};
