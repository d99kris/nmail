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
    FlagChecking = (1 << 1),
    FlagFetching = (1 << 2),
    FlagSending = (1 << 3),
    FlagPrefetching = (1 << 4),
    FlagMoving = (1 << 5),
    FlagDeleting = (1 << 6),
    FlagUpdatingFlags = (1 << 7),
    FlagSaving = (1 << 8),
    FlagConnected = (1 << 9),
    FlagOffline = (1 << 10),
    FlagIdle = (1 << 11),
    FlagIndexing = (1 << 12),
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
