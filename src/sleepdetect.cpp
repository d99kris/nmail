// sleepdetect.cpp
//
// Copyright (c) 2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "sleepdetect.h"

#include "log.h"
#include "loghelp.h"

SleepDetect::SleepDetect(const std::function<void()>& p_OnWakeUp, int p_MinSleepSec)
  : m_OnWakeUp(p_OnWakeUp)
  , m_MinSleepSec(std::max(p_MinSleepSec, 1))
{
  LOG_DEBUG_FUNC(STR(p_MinSleepSec));

  m_Running = true;
  m_Thread = std::thread(&SleepDetect::Process, this);
  LOG_DEBUG("thread started");
}

SleepDetect::~SleepDetect()
{
  LOG_DEBUG_FUNC(STR());

  if (m_Running)
  {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_Running = false;
    m_CondVar.notify_one();
  }

  if (m_Thread.joinable())
  {
    m_Thread.join();
  }

  LOG_DEBUG("thread stopped");
}

void SleepDetect::Process()
{
  LOG_DEBUG("start process");

  const int intervalSec = std::max(1, (m_MinSleepSec / 10));
  auto lastTime = std::chrono::system_clock::now();
  while (m_Running)
  {
    auto nowTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsedSec = nowTime - lastTime;
    lastTime = nowTime;

    if (elapsedSec.count() > (intervalSec + m_MinSleepSec))
    {
      m_OnWakeUp();
    }

    std::unique_lock<std::mutex> lock(m_Mutex);
    m_CondVar.wait_for(lock, std::chrono::seconds(intervalSec));

    if (!m_Running)
    {
      lock.unlock();
      break;
    }
  }

  LOG_DEBUG("exit process");
}
