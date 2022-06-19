// version.h
//
// Copyright (c) 2022 Kristofer Berggren
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
  static std::string GetMessageIdAppVersion();
  static std::string GetUiAppVersion();
};
