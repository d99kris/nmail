// debuginfo.cpp
//
// Copyright (c) 2025-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "debuginfo.h"

#include <map>

#include "apputil.h"

std::shared_ptr<Config> DebugInfo::m_Config;

void DebugInfo::Init()
{
  const std::map<std::string, std::string> defaultConfig =
  {
    { "version_used", "" },
  };

  const std::string configPath(AppUtil::GetApplicationDir() + std::string("/debug.info"));
  m_Config.reset(new Config(configPath, defaultConfig));
}

void DebugInfo::Cleanup()
{
  m_Config->Save();
  m_Config.reset();
}

std::string DebugInfo::GetStr(const std::string& p_Param)
{
  return m_Config->Get(p_Param);
}

void DebugInfo::SetStr(const std::string& p_Param, const std::string& p_Value)
{
  m_Config->Set(p_Param, p_Value);
}
