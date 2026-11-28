// uicolorconfig.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>

class UiColorConfig
{
public:
  static void Init();
  static int GetAttrs(const std::string& p_Item); // ex: "color_dialog"

private:
  static std::map<std::string, int> m_Attrs;
};
