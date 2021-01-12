// cacheutil.cpp
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "cacheutil.h"

#include "crypto.h"
#include "loghelp.h"
#include "serialized.h"
#include "util.h"

bool CacheUtil::CommonInitCacheDir(const std::string& p_Dir, int p_Version, bool p_Encrypted)
{
  const std::string& versionPath = p_Dir + "version";
  const int currentVersion = (p_Version * 10) + (p_Encrypted ? 1 : 0);
  if (Util::Exists(p_Dir))
  {
    int storedVersion = -1;
    DeserializeFromFile(versionPath, storedVersion);
    if (storedVersion != currentVersion)
    {
      LOG_DEBUG("re-init %s", p_Dir.c_str());
      Util::RmDir(p_Dir);
      Util::MkDir(p_Dir);
      SerializeToFile(versionPath, currentVersion);
      return false;
    }
  }
  else
  {
    LOG_DEBUG("init %s", p_Dir.c_str());
    Util::MkDir(p_Dir);
    SerializeToFile(versionPath, currentVersion);
  }

  return true;
}

void CacheUtil::DecryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = Util::ListDir(p_SrcDir);
  for (auto& file : files)
  {
    Crypto::AESDecryptFile(p_SrcDir + "/" + file, p_DstDir + "/" + file, p_Pass);
  }
}

void CacheUtil::EncryptCacheDir(const std::string& p_Pass, const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = Util::ListDir(p_SrcDir);
  for (auto& file : files)
  {
    Crypto::AESEncryptFile(p_SrcDir + "/" + file, p_DstDir + "/" + file, p_Pass);
  }
}
