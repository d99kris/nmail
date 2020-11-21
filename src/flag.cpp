// flag.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "flag.h"

bool Flag::GetSeen(uint32_t p_Flag)
{
  return (p_Flag & Seen);
}

void Flag::SetSeen(uint32_t& p_Flags, bool p_Seen)
{
  p_Flags = p_Seen ? (p_Flags | Seen) : (p_Flags & ~Seen);
}
