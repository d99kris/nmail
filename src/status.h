// status.h
//
// Copyright (c) 2019-2020 Kristofer Berggren
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
    FlagFetching = (1 << 1),
    FlagSending = (1 << 2),
    FlagPrefetching = (1 << 3),
    FlagMoving = (1 << 4),
    FlagDeleting = (1 << 5),
    FlagUpdatingFlags = (1 << 6),
    FlagSaving = (1 << 7),
    FlagConnected = (1 << 8),
    FlagOffline = (1 << 9),
    FlagIdle = (1 << 10),
    FlagIndexing = (1 << 11),
    FlagMax = FlagIdle,
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
