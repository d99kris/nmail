// mainconfig.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>

#include "config.h"

class MainConfig
{
public:
  static void Init();
  static void Cleanup();
  static std::string GetPath();
  static const std::map<std::string, std::string>& GetDefaultConfig();
  static std::shared_ptr<Config> GetConfig();
  static bool GetBool(const std::string& p_Param);
  static std::string GetStr(const std::string& p_Param);
  static void SetStr(const std::string& p_Param, const std::string& p_Value);
  static void Save();
  static void LogParamsExcept(const std::set<std::string>& p_Exclude);

private:
  static std::shared_ptr<Config> m_Config;
};
