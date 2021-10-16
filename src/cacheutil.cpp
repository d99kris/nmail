// cacheutil.cpp
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "cacheutil.h"

#include "crypto.h"
#include "loghelp.h"
#include "util.h"

void CacheUtil::InitCacheDir()
{
  static const int version = 4;
  const std::string cacheDir = GetCacheDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, false /* p_Encrypted */);
}

std::string CacheUtil::GetCacheDir()
{
  return Util::GetApplicationDir() + std::string("cache/");
}

bool CacheUtil::CommonInitCacheDir(const std::string& p_Dir, int p_Version, bool p_Encrypted)
{
  const std::string& versionPath = p_Dir + "version";
  const int currentVersion = (p_Version * 10) + (p_Encrypted ? 1 : 0);
  if (Util::Exists(p_Dir))
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
      Util::RmDir(p_Dir);
      Util::MkDir(p_Dir);
      WriteVersionToFile(versionPath, currentVersion);
      return false;
    }
  }
  else
  {
    LOG_DEBUG("init %s", p_Dir.c_str());
    Util::MkDir(p_Dir);
    WriteVersionToFile(versionPath, currentVersion);
  }

  return true;
}

bool CacheUtil::DecryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = Util::ListDir(p_SrcDir);
  for (auto& file : files)
  {
    if (!Crypto::AESDecryptFile(p_SrcDir + "/" + file, p_DstDir + "/" + file, p_Pass))
    {
      Util::DeleteFile(p_DstDir + "/" + file);
      return false;
    }
  }

  return true;
}

bool CacheUtil::EncryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = Util::ListDir(p_SrcDir);
  for (auto& file : files)
  {
    if (!Crypto::AESEncryptFile(p_SrcDir + "/" + file, p_DstDir + "/" + file, p_Pass))
    {
      Util::DeleteFile(p_DstDir + "/" + file);
      return false;
    }
  }

  return true;
}

void CacheUtil::ReadVersionFromFile(const std::string& p_Path, int& p_Version)
{
  std::string str = Util::FromHex(Util::ReadFile(p_Path));
  if (Util::IsInteger(str))
  {
    p_Version = Util::ToInteger(str);
  }
}

void CacheUtil::WriteVersionToFile(const std::string& p_Path, const int p_Version)
{
  Util::WriteFile(p_Path, Util::ToHex(std::to_string(p_Version)));
}
