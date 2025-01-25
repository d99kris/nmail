// config.h
//
// Copyright (c) 2019-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <set>
#include <string>

class Config
{
public:
  Config();
  Config(const std::string& p_Path, const std::map<std::string, std::string>& p_Default);
  virtual ~Config();

  void Load(const std::string& p_Path);
  void Save() const;
  void Save(const std::string& p_Path) const;
  std::string Get(const std::string& p_Param) const;
  void Set(const std::string& p_Param, const std::string& p_Value);
  void Delete(const std::string& p_Param);
  bool Exist(const std::string& p_Param);
  std::map<std::string, std::string> GetMap() const;

  void LogParams() const;
  void LogParamsExcept(const std::set<std::string>& p_Exclude) const;

private:
  std::map<std::string, std::string> m_Map;
  std::string m_Path;
};
