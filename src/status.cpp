// status.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "status.h"

#include <iomanip>

#include "loghelp.h"
#include "util.h"

Status::Status()
{
}

Status::~Status()
{
}

void Status::SetShowProgress(int p_ShowProgress)
{
  m_ShowProgress = p_ShowProgress;
}

void Status::Update(const StatusUpdate& p_StatusUpdate)
{
  m_Flags |= p_StatusUpdate.SetFlags;
  m_Flags &= ~p_StatusUpdate.ClearFlags;
  if (p_StatusUpdate.Progress >= 0)
  {
    m_Progress = p_StatusUpdate.Progress;
  }
}

bool Status::IsSet(const Status::Flag& p_Flag)
{
  return m_Flags & p_Flag;
}

std::string Status::ToString()
{
  std::string str;

  if (m_Flags & FlagConnecting)
  {
    str = "Connecting";
  }
  else if (m_Flags & FlagDisconnecting)
  {
    str = "Disconnecting";
  }
  else if (m_Flags & FlagExiting)
  {
    str = "Exiting";
  }
  else if (m_Flags & FlagChecking)
  {
    str = "Checking";
  }
  else if (m_Flags & FlagFetching)
  {
    str = "Fetching" + GetProgressString();
  }
  else if (m_Flags & FlagSending)
  {
    str = "Sending";
  }
  else if (m_Flags & FlagPrefetching)
  {
    str = "Pre-fetching" + GetProgressString();
  }
  else if (m_Flags & FlagMoving)
  {
    str = "Moving";
  }
  else if (m_Flags & FlagDeleting)
  {
    str = "Deleting";
  }
  else if (m_Flags & FlagUpdatingFlags)
  {
    str = "Updating flags";
  }
  else if (m_Flags & FlagSaving)
  {
    str = "Saving";
  }
  else if (m_Flags & FlagIndexing)
  {
    str = "Indexing" + GetProgressString();
  }
  else if (m_Flags & FlagIdle)
  {
    str = "Idle";
  }
  else if (m_Flags & FlagConnected)
  {
    str = "Connected";
  }
  else if (m_Flags & FlagOffline)
  {
    str = "Offline";
  }
  else if (m_Flags & FlagNone)
  {
    str = "No status";
  }
  else
  {
    str = "inv status";
  }

  static std::string lastStr;
  if (str != lastStr)
  {
    LOG_DEBUG("new status: %s", str.c_str());
    lastStr = str;
  }

  return str;
}

std::string Status::GetProgressString()
{
  if (m_ShowProgress == 0) return "";

  if (m_Progress < 0.0001) return "";

  if (m_ShowProgress == 1)
  {
    static int precision = 0;
    struct timeval nowTv;
    gettimeofday(&nowTv, NULL);
    static struct timeval lastTv = nowTv;

    static float progressDiffSum = 0;
    static float progressDiffCount = 0;
    static const int targetProgressUpdateIntervalSecs = 2;
    if ((nowTv.tv_sec - lastTv.tv_sec) >= targetProgressUpdateIntervalSecs)
    {
      static float lastProgress = m_Progress;
      float progressDiff = fabsf(m_Progress - lastProgress);
      progressDiffSum += progressDiff;
      progressDiffCount += 1;

      int newPrecision = precision;
      static struct timeval lastPrecisionUpdateTv = nowTv;

      static const int minIntervalBetweenPrecisionUpdateSecs = 10;
      if ((nowTv.tv_sec - lastPrecisionUpdateTv.tv_sec) >= minIntervalBetweenPrecisionUpdateSecs)
      {
        float progressDiffLog10 = log10(progressDiffSum / progressDiffCount);
        progressDiffSum = 0;
        progressDiffCount = 0;

        int targetPrecision = 0;
        if (progressDiffLog10 >= 0)
        {
          targetPrecision = 0;
        }
        else
        {
          targetPrecision = -floorf(progressDiffLog10);
        }

        if (targetPrecision > newPrecision)
        {
          ++newPrecision;
        }
        else if (targetPrecision < newPrecision)
        {
          --newPrecision;
        }

        static const int maxPrecision = 4;
        newPrecision = Util::Bound(0, newPrecision, maxPrecision);
        if (newPrecision != precision)
        {
          precision = newPrecision;
          lastPrecisionUpdateTv = nowTv;
        }
      }

      lastTv = nowTv;
      lastProgress = m_Progress;
    }

    std::stringstream stream;
    float floorProgress = floorf(m_Progress * powf(10, precision)) / powf(10, precision);
    stream << " " << std::fixed << std::setprecision(precision) << floorProgress << "%";
    return stream.str();
  }

  if (m_ShowProgress == 2) return " " + std::to_string((int)m_Progress) + "%";

  return "";
}
