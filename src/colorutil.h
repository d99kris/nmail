// colorutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstdint>
#include <string>

class ColorUtil
{
public:
  static void HexToRGB(const std::string p_Str, uint32_t& p_R, uint32_t& p_G, uint32_t& p_B);
  static int GetColor(const std::string& p_Str);
  static int GetColorAttrs(const std::string& p_FgStr, const std::string& p_BgStr);
  static int GetAttrs(const std::string& p_AttrStr);
};
