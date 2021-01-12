// status.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "status.h"

#include "loghelp.h"

Status::Status()
{
}

Status::~Status()
{
}

void Status::Update(const StatusUpdate& p_StatusUpdate)
{
  m_Flags |= p_StatusUpdate.SetFlags;
  m_Flags &= ~p_StatusUpdate.ClearFlags;
  if (p_StatusUpdate.Progress != -1)
  {
    m_Progress = p_StatusUpdate.Progress;
  }
}

bool Status::IsSet(const Status::Flag& p_Flag)
{
  return m_Flags & p_Flag;
}

std::string Status::ToString(bool p_ShowProgress)
{
  std::string str;

  if (m_Flags & FlagConnecting)
  {
    str = "Connecting";
  }
  else if (m_Flags & FlagFetching)
  {
    if (p_ShowProgress && (m_Progress > 0))
    {
      str = "Fetching " + std::to_string(m_Progress) + "%";
    }
    else
    {
      str = "Fetching";
    }
  }
  else if (m_Flags & FlagSending)
  {
    str = "Sending";
  }
  else if (m_Flags & FlagPrefetching)
  {
    if (p_ShowProgress && (m_Progress > 0))
    {
      str = "Pre-fetching " + std::to_string(m_Progress) + "%";
    }
    else
    {
      str = "Pre-fetching";
    }
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
    if (p_ShowProgress && (m_Progress > 0))
    {
      str = "Indexing " + std::to_string(m_Progress) + "%";
    }
    else
    {
      str = "Indexing";
    }
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
