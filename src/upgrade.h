// upgrade.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

#include <sys/types.h>

class Upgrade
{
public:
  static int Run(const std::string& p_Version, mode_t p_Umask);
};
