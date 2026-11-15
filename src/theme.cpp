// theme.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "theme.h"

#include <fstream>
#include <iostream>

#include "apathy/path.hpp"

#include "themes.h"
#include "util.h"

static const size_t themeCount = sizeof(EmbeddedThemes) / sizeof(EmbeddedThemes[0]);

bool Theme::Install(const std::string& p_Name)
{
  size_t index = 0;
  if (p_Name.empty())
  {
    if (!Select(index)) return false;
  }
  else if (!Find(p_Name, index))
  {
    std::cerr << "error: theme \"" << p_Name << "\" does not exist, run \"nmail --theme\" for a list\n"
      "       of available themes.\n";
    return false;
  }

  const std::string appDir = Util::GetApplicationDir();
  if (!apathy::Path(appDir).exists())
  {
    apathy::Path::makedirs(appDir);
  }

  const std::string path = appDir + "colors.conf";
  std::ofstream file(path, std::ios::binary);
  file << GetData(index);
  if (!file.good())
  {
    std::cerr << "error: failed to write " << path << "\n";
    return false;
  }

  std::cout << "Installed theme \"" << EmbeddedThemes[index].name << "\" to " << path << "\n";
  return true;
}

bool Theme::Select(size_t& p_Index)
{
  // first theme in index.txt is the default selection
  std::cout << "Themes:\n";
  for (size_t i = 0; i < themeCount; ++i)
  {
    std::cout << (i + 1) << ". " << EmbeddedThemes[i].name << "\n";
  }

  std::cout << "Press ctrl-c to cancel.\n";

  std::cout << "Select theme (1): ";
  std::string line;
  if (!std::getline(std::cin, line))
  {
    std::cout << "\n";
    return false;
  }

  line = Util::Trim(line);
  if (line.empty())
  {
    p_Index = 0;
    return true;
  }

  const bool isNumber = (line.find_first_not_of("0123456789") == std::string::npos) && (line.size() < 6);
  const size_t number = isNumber ? std::stoul(line) : 0;
  if ((number < 1) || (number > themeCount))
  {
    std::cerr << "error: invalid selection \"" << line << "\".\n";
    return false;
  }

  p_Index = number - 1;
  return true;
}

bool Theme::Find(const std::string& p_Name, size_t& p_Index)
{
  for (size_t i = 0; i < themeCount; ++i)
  {
    if (p_Name == EmbeddedThemes[i].name)
    {
      p_Index = i;
      return true;
    }
  }

  return false;
}

std::string Theme::GetData(size_t p_Index)
{
  return std::string(reinterpret_cast<const char*>(EmbeddedThemes[p_Index].data), EmbeddedThemes[p_Index].size);
}
