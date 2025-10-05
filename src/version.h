// version.h
//
// Copyright (c) 2022-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class Version
{
public:
  static std::string GetBuildOs();
  static std::string GetCompiler();
  static std::string GetAppName(bool p_WithVersion);
  static std::string GetAppVersion();
};
