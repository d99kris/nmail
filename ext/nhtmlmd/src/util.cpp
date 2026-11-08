// util.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#include "util.h"

#include <cctype>

namespace Nhtmlmd
{
  size_t Util::DecodeUtf8(const std::string& p_Str, size_t p_Pos, uint32_t& p_Cp)
  {
    const unsigned char c0 = (unsigned char)p_Str[p_Pos];
    const size_t left = p_Str.size() - p_Pos;
    if ((c0 < 0x80) || (left < 2))
    {
      p_Cp = c0;
      return 1;
    }

    if ((c0 & 0xE0) == 0xC0)
    {
      p_Cp = ((c0 & 0x1F) << 6) | (p_Str[p_Pos + 1] & 0x3F);
      return 2;
    }

    if (((c0 & 0xF0) == 0xE0) && (left >= 3))
    {
      p_Cp = ((c0 & 0x0F) << 12) | ((p_Str[p_Pos + 1] & 0x3F) << 6) | (p_Str[p_Pos + 2] & 0x3F);
      return 3;
    }

    if (((c0 & 0xF8) == 0xF0) && (left >= 4))
    {
      p_Cp = ((c0 & 0x07) << 18) | ((p_Str[p_Pos + 1] & 0x3F) << 12) |
        ((p_Str[p_Pos + 2] & 0x3F) << 6) | (p_Str[p_Pos + 3] & 0x3F);
      return 4;
    }

    p_Cp = c0;
    return 1;
  }

  // invisible padding used by email templates
  bool Util::IsJunk(uint32_t p_Cp)
  {
    return (p_Cp == 0x034F) || (p_Cp == 0x00AD) || ((p_Cp >= 0x200B) && (p_Cp <= 0x200F)) ||
           (p_Cp == 0x2060) || (p_Cp == 0xFEFF) || (p_Cp == 0x180E);
  }

  bool Util::IsSpace(uint32_t p_Cp)
  {
    return (p_Cp == ' ') || (p_Cp == '\t') || (p_Cp == '\n') || (p_Cp == '\r') ||
           (p_Cp == '\f') || (p_Cp == 0x00A0) || (p_Cp == 0x2007) || (p_Cp == 0x202F) ||
           (p_Cp == 0x3000) || ((p_Cp >= 0x2000) && (p_Cp <= 0x200A));
  }

  size_t Util::DisplayWidth(const std::string& p_Str)
  {
    size_t width = 0;
    for (size_t i = 0; i < p_Str.size(); )
    {
      uint32_t cp = 0;
      i += DecodeUtf8(p_Str, i, cp);
      const bool wide = (cp >= 0x1100) &&
        ((cp <= 0x115F) || ((cp >= 0x2E80) && (cp <= 0xA4CF)) || ((cp >= 0xAC00) && (cp <= 0xD7A3)) ||
         ((cp >= 0xF900) && (cp <= 0xFAFF)) || ((cp >= 0xFF00) && (cp <= 0xFF60)) ||
         ((cp >= 0x1F300) && (cp <= 0x1FAFF)));
      width += wide ? 2 : 1;
    }

    return width;
  }

  std::vector<std::string> Util::SplitLines(const std::string& p_Str)
  {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= p_Str.size())
    {
      size_t end = p_Str.find('\n', start);
      if (end == std::string::npos) end = p_Str.size();
      lines.push_back(p_Str.substr(start, end - start));
      start = end + 1;
    }

    return lines;
  }

  std::string Util::ToLower(std::string p_Str)
  {
    for (auto& ch : p_Str)
    {
      ch = (char)tolower((unsigned char)ch);
    }

    return p_Str;
  }

  std::string Util::StripSpaces(const std::string& p_Str)
  {
    std::string str;
    for (char ch : p_Str)
    {
      if (!isspace((unsigned char)ch)) str += (char)tolower((unsigned char)ch);
    }

    return str;
  }
}
