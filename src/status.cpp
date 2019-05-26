// status.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "status.h"

Status::Status()
{
}

Status::~Status()
{
}

void Status::Update(const StatusUpdate &p_StatusUpdate)
{
  m_Flags |= p_StatusUpdate.SetFlags;
  m_Flags &= ~p_StatusUpdate.ClearFlags;
  m_Progress = p_StatusUpdate.Progress;
}

bool Status::IsSet(const Status::Flag &p_Flag)
{
  return m_Flags & p_Flag;
}

std::string Status::ToString(bool p_ShowProgress)
{
  if (m_Flags & FlagConnecting)
  {
    return "Connecting";
  }
  else if (m_Flags & FlagFetching)
  {
    if (p_ShowProgress && (m_Progress > 0))
    {
      return "Fetching " + std::to_string(m_Progress) + "%";
    }
    else
    {
      return "Fetching";
    }
  }
  else if (m_Flags & FlagSending)
  {
    return "Sending";
  }
  else if (m_Flags & FlagPrefetching)
  {
    if (p_ShowProgress && (m_Progress > 0))
    {
      return "Pre-fetching " + std::to_string(m_Progress) + "%";
    }
    else
    {
      return "Pre-fetching";
    }
  }
  else if (m_Flags & FlagMoving)
  {
    return "Moving";
  }
  else if (m_Flags & FlagDeleting)
  {
    return "Deleting";
  }
  else if (m_Flags & FlagUpdatingFlags)
  {
    return "Updating flags";
  }
  else if (m_Flags & FlagIdle)
  {
    return "Idle";
  }
  else if (m_Flags & FlagConnected)
  {
    return "Connected";
  }
  else if (m_Flags & FlagOffline)
  {
    return "Offline";
  }
  else if (m_Flags & FlagNone)
  {
    return "No status";
  }
  else
  {
    return "inv status";
  }
}
