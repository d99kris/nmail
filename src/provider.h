// provider.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>
#include <vector>

class Provider
{
public:
  enum MxResult
  {
    MxResultMatch = 0,
    MxResultNoMatch,
    MxResultFailed,
    MxResultUnavailable,
  };

  static std::vector<std::string> GetIds();
  static bool Exists(const std::string& p_Id);
  static std::map<std::string, std::string> GetParams(const std::string& p_Id);
  static std::string FindByDomain(const std::string& p_Domain);
  static std::string FindByMx(const std::string& p_Domain, MxResult& p_MxResult);
  static bool HasMxLookup();

private:
  static bool GetMxHosts(const std::string& p_Domain, std::vector<std::string>& p_Hosts);
  static bool IsDomainOrSubdomain(const std::string& p_Host, const std::string& p_Domain);
};
