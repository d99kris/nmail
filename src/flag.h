// flag.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstdint>

class Flag
{
public:
  static bool GetSeen(uint32_t p_Flag);
  static void SetSeen(uint32_t& p_Flags, bool p_Seen);

public:
  static const uint32_t Seen = 1 << 0;
};
