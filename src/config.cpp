// config.cpp
//
// Copyright (c) 2019-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "config.h"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include <sys/stat.h>

#include "loghelp.h"
#include "util.h"

Config::Config()
{
}

Config::Config(const std::string& p_Path,
               const std::map<std::string, std::string>& p_Default)
  : m_Map(p_Default)
{
  Load(p_Path);
}

Config::~Config()
{
}

void Config::Load(const std::string& p_Path)
{
  m_Path = p_Path;

  std::ifstream stream;
  stream.open(p_Path, std::ios::binary);
  if (stream.fail())
  {
    Save();
    chmod(p_Path.c_str(), 0600);
    return;
  }

  const bool hasDefaultMap = !m_Map.empty();
  std::string line;
  while (std::getline(stream, line))
  {
    if (line.length() == 0) continue;

    if (line[0] == '#') continue;

    std::string param;
    std::istringstream linestream(line);
    if (!std::getline(linestream, param, '=')) continue;

    std::string value;
    std::getline(linestream, value);
    param = Util::Trim(param);

    if (hasDefaultMap && (m_Map.count(param) == 0))
    {
      // drop params not present in default map
      LOG_WARNING("unknown param \"%s\"", param.c_str());
      continue;
    }

    m_Map[param] = Util::Trim(value);
  }
}

void Config::Save() const
{
  Save(m_Path);
}

void Config::Save(const std::string& p_Path) const
{
  std::ofstream stream;
  stream.open(p_Path, std::ios::binary);
  if (stream.fail())
  {
    return;
  }

  for (auto const& item : m_Map)
  {
    stream << item.first << "=" << item.second << std::endl;
  }
}

std::string Config::Get(const std::string& p_Param) const
{
  return m_Map.at(p_Param);
}

void Config::Set(const std::string& p_Param, const std::string& p_Value)
{
  m_Map[p_Param] = p_Value;
}

void Config::Delete(const std::string& p_Param)
{
  m_Map.erase(p_Param);
}

bool Config::Exist(const std::string& p_Param)
{
  return (m_Map.find(p_Param) != m_Map.end());
}

std::map<std::string, std::string> Config::GetMap() const
{
  return m_Map;
}

void Config::LogParams() const
{
  LOG_DEBUG("config %s", m_Path.c_str());
  for (auto const& item : m_Map)
  {
    LOG_DEBUG("%s=%s", item.first.c_str(), item.second.c_str());
  }
}

void Config::LogParamsExcept(const std::set<std::string>& p_Exclude) const
{
  LOG_DEBUG("config %s", m_Path.c_str());
  for (auto const& item : m_Map)
  {
    if (p_Exclude.find(item.first) == p_Exclude.end())
    {
      LOG_DEBUG("%s=%s", item.first.c_str(), item.second.c_str());
    }
    else
    {
      std::string placeholder = std::string(item.second.size(), '*');
      LOG_DEBUG("%s=%s", item.first.c_str(), placeholder.c_str());
    }
  }
}
