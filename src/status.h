// status.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstdint>
#include <string>

struct StatusUpdate
{
  uint32_t SetFlags = 0;
  uint32_t ClearFlags = 0;
  int32_t Progress = -1;
};

class Status
{
public:
  enum Flag
  {
    FlagNone = 0,
    FlagConnecting = (1 << 0),
    FlagDisconnecting = (1 << 1),
    FlagExiting = (1 << 2),
    FlagChecking = (1 << 3),
    FlagFetching = (1 << 4),
    FlagSending = (1 << 5),
    FlagPrefetching = (1 << 6),
    FlagMoving = (1 << 7),
    FlagDeleting = (1 << 8),
    FlagUpdatingFlags = (1 << 9),
    FlagSaving = (1 << 10),
    FlagConnected = (1 << 11),
    FlagOffline = (1 << 12),
    FlagIdle = (1 << 13),
    FlagIndexing = (1 << 14),
    FlagMax = FlagIndexing,
  };

  Status();
  virtual ~Status();

  void Update(const StatusUpdate& p_StatusUpdate);
  bool IsSet(const Flag& p_Flag);
  std::string ToString(bool p_ShowProgress);

private:
  uint32_t m_Flags = 0;
  int32_t m_Progress = 0;
};
