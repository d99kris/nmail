// cacheutil.cpp
//
// Copyright (c) 2020-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "cacheutil.h"

#include "apputil.h"
#include "crypto.h"
#include "fileutil.h"
#include "loghelp.h"
#include "strutil.h"

void CacheUtil::InitCacheDir()
{
  static const int version = 5;
  const std::string cacheDir = GetCacheDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, false /* p_Encrypted */);
}

std::string CacheUtil::GetCacheDir()
{
  return AppUtil::GetApplicationDir() + std::string("cache/");
}

bool CacheUtil::CommonInitCacheDir(const std::string& p_Dir, int p_Version, bool p_Encrypted)
{
  const std::string& versionPath = p_Dir + "version";
  const int currentVersion = (p_Version * 10) + (p_Encrypted ? 1 : 0);
  if (FileUtil::Exists(p_Dir))
  {
    int storedVersion = -1;
    try
    {
      ReadVersionFromFile(versionPath, storedVersion);
    }
    catch (...)
    {
      LOG_WARNING("failed to deserialize %s", versionPath.c_str());
      storedVersion = -1;
    }

    if (storedVersion != currentVersion)
    {
      LOG_DEBUG("re-init %s", p_Dir.c_str());
      FileUtil::RmDir(p_Dir);
      FileUtil::MkDir(p_Dir);
      WriteVersionToFile(versionPath, currentVersion);
      return false;
    }
  }
  else
  {
    LOG_DEBUG("init %s", p_Dir.c_str());
    FileUtil::MkDir(p_Dir);
    WriteVersionToFile(versionPath, currentVersion);
  }

  return true;
}

bool CacheUtil::DecryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = FileUtil::ListDir(p_SrcDir);
  for (auto& file : files)
  {
    if (!Crypto::AESDecryptFile(p_SrcDir + "/" + file, p_DstDir + "/" + file, p_Pass))
    {
      FileUtil::DeleteFile(p_DstDir + "/" + file);
      return false;
    }
  }

  return true;
}

bool CacheUtil::EncryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = FileUtil::ListDir(p_SrcDir);
  for (auto& file : files)
  {
    if (!Crypto::AESEncryptFile(p_SrcDir + "/" + file, p_DstDir + "/" + file, p_Pass))
    {
      FileUtil::DeleteFile(p_DstDir + "/" + file);
      return false;
    }
  }

  return true;
}

void CacheUtil::ReadVersionFromFile(const std::string& p_Path, int& p_Version)
{
  std::string str = StrUtil::FromHex(FileUtil::ReadFile(p_Path));
  if (StrUtil::IsInteger(str))
  {
    p_Version = StrUtil::ToInteger(str);
  }
}

void CacheUtil::WriteVersionToFile(const std::string& p_Path, const int p_Version)
{
  FileUtil::WriteFile(p_Path, StrUtil::ToHex(std::to_string(p_Version)));
}
