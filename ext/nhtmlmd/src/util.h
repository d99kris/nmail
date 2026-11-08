// util.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Nhtmlmd
{
  class Util
  {
  public:
    static size_t DecodeUtf8(const std::string& p_Str, size_t p_Pos, uint32_t& p_Cp);
    static bool IsJunk(uint32_t p_Cp);
    static bool IsSpace(uint32_t p_Cp);
    static size_t DisplayWidth(const std::string& p_Str);
    static std::vector<std::string> SplitLines(const std::string& p_Str);
    static std::string ToLower(std::string p_Str);
    static std::string StripSpaces(const std::string& p_Str);
  };
}
