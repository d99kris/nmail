// cacheutil.h
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class CacheUtil
{
public:
  static void InitCacheDir();
  static std::string GetCacheDir();

  static bool CommonInitCacheDir(const std::string& p_Dir, int p_Version, bool p_Encrypted);
  static bool DecryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir);
  static bool EncryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir);
  static void ReadVersionFromFile(const std::string& p_Path, int& p_Version);
  static void WriteVersionToFile(const std::string& p_Path, const int p_Version);
};
