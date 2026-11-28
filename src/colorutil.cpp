// colorutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "colorutil.h"

#include <map>
#include <sstream>

#include <ncurses.h>

#include "loghelp.h"
#include "strutil.h"

void ColorUtil::HexToRGB(const std::string p_Str, uint32_t& p_R, uint32_t& p_G, uint32_t& p_B)
{
  std::stringstream ss(p_Str);
  int val;
  ss >> std::hex >> val;

  p_R = (val / 0x10000);
  p_G = (val / 0x100) % 0x100;
  p_B = (val % 0x100);
}

int ColorUtil::GetColor(const std::string& p_Str)
{
  static const std::map<std::string, int> standardColors = []()
  {
    std::map<std::string, int> colors;
    const std::map<std::string, int> basicColors =
    {
      { "black", COLOR_BLACK },
      { "red", COLOR_RED },
      { "green", COLOR_GREEN },
      { "yellow", COLOR_YELLOW },
      { "blue", COLOR_BLUE },
      { "magenta", COLOR_MAGENTA },
      { "cyan", COLOR_CYAN },
      { "white", COLOR_WHITE },
    };
    colors.insert(basicColors.begin(), basicColors.end());

    if (COLORS > 8)
    {
      const int BRIGHT = 8;
      const std::map<std::string, int> extendedColors =
      {
        { "gray", BRIGHT | COLOR_BLACK },
        { "bright_black", BRIGHT | COLOR_BLACK },
        { "bright_red", BRIGHT | COLOR_RED },
        { "bright_green", BRIGHT | COLOR_GREEN },
        { "bright_yellow", BRIGHT | COLOR_YELLOW },
        { "bright_blue", BRIGHT | COLOR_BLUE },
        { "bright_magenta", BRIGHT | COLOR_MAGENTA },
        { "bright_cyan", BRIGHT | COLOR_CYAN },
        { "bright_white", BRIGHT | COLOR_WHITE },
      };
      colors.insert(extendedColors.begin(), extendedColors.end());
    }

    return colors;
  }();

  if (p_Str.empty() || (p_Str == "normal")) return -1;

  // hex
  if ((p_Str.size() == 8) && (p_Str.substr(0, 2) == "0x"))
  {
    if (!can_change_color())
    {
      LOG_WARNING("terminal cannot set custom hex colors, skipping \"%s\"", p_Str.c_str());
      return -1;
    }

    uint32_t r = 0, g = 0, b = 0;
    ColorUtil::HexToRGB(p_Str, r, g, b);
    if ((r <= 255) && (g <= 255) && (b <= 255))
    {
      static int colorId = 31;
      colorId++;
      if (colorId > COLORS)
      {
        LOG_WARNING("max number of colors (%d) already defined, skipping \"%s\"", p_Str.c_str());
        return -1;
      }

      init_color(colorId, ((r * 1000) / 255), ((g * 1000) / 255), ((b * 1000) / 255));
      return colorId;
    }

    LOG_WARNING("invalid color hex code \"%s\"", p_Str.c_str());
    return -1;
  }

  // name
  std::map<std::string, int>::const_iterator standardColor = standardColors.find(p_Str);
  if (standardColor != standardColors.end())
  {
    return standardColor->second;
  }

  // id
  if (StrUtil::IsInteger(p_Str))
  {
    int32_t id = StrUtil::ToInteger(p_Str);
    return id;
  }

  if (p_Str == "reverse")
  {
    LOG_WARNING("both fg and bg must be set to \"reverse\"", p_Str.c_str());
    return -1;
  }

  LOG_WARNING("unsupported color string \"%s\"", p_Str.c_str());
  return -1;
}

int ColorUtil::GetColorAttrs(const std::string& p_FgStr, const std::string& p_BgStr)
{
  if (p_FgStr.empty() && p_BgStr.empty()) return A_NORMAL;

  if ((p_FgStr == "normal") && (p_BgStr == "normal")) return A_NORMAL;

  if ((p_FgStr == "reverse") && (p_BgStr == "reverse")) return A_REVERSE;

  const int fgColor = GetColor(p_FgStr);
  const int bgColor = GetColor(p_BgStr);
  if ((fgColor == -1) && (bgColor == -1)) return A_NORMAL;

  static int colorPairId = 0;
  colorPairId++;
  init_pair(colorPairId, fgColor, bgColor);
  return COLOR_PAIR(colorPairId);
}

int ColorUtil::GetAttrs(const std::string& p_AttrStr)
{
  if (p_AttrStr.empty() || (p_AttrStr == "normal")) return A_NORMAL;

  if (p_AttrStr == "bold") return A_BOLD;

  if (p_AttrStr == "dim") return A_DIM;

#ifdef A_ITALIC
  if (p_AttrStr == "italic") return (termattrs() & A_ITALIC) ? A_ITALIC : A_UNDERLINE;
#else
  if (p_AttrStr == "italic") return A_UNDERLINE;
#endif

  if (p_AttrStr == "reverse") return A_REVERSE;

  if (p_AttrStr == "underline") return A_UNDERLINE;

  LOG_WARNING("unsupported attr string \"%s\"", p_AttrStr.c_str());
  return A_NORMAL;
}
