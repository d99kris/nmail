// imapcache.cpp
//
// Copyright (c) 2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imapcache.h"

#include "cacheutil.h"
#include "crypto.h"
#include "loghelp.h"
#include "util.h"
#include "serialized.h"

ImapCache::ImapCache(const bool p_CacheEncrypt, const std::string& p_Pass)
  : m_CacheEncrypt(p_CacheEncrypt)
  , m_Pass(p_Pass)
{
  InitCacheDir();
  InitImapCacheDir();
}

ImapCache::~ImapCache()
{
}

std::string ImapCache::GetCacheDir()
{
  return Util::GetApplicationDir() + std::string("cache/");
}

void ImapCache::InitCacheDir()
{
  static const int version = 1;
  const std::string cacheDir = GetCacheDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, false /* p_Encrypted */);
}

std::string ImapCache::GetImapCacheDir()
{
  return GetCacheDir() + std::string("imap/");
}

void ImapCache::InitImapCacheDir()
{
  static const int version = 1;
  const std::string imapCacheDir = GetImapCacheDir();
  CacheUtil::CommonInitCacheDir(imapCacheDir, version, m_CacheEncrypt);
}

std::string ImapCache::GetFolderCacheDir(const std::string &p_Folder)
{
  if (m_CacheEncrypt)
  {
    // @todo: consider encrypting folder name instead of hashing
    return GetImapCacheDir() + Crypto::SHA256(p_Folder) + std::string("/");
  }
  else
  {
    return GetImapCacheDir() + Serialized::ToHex(p_Folder) + std::string("/");
  }
}

std::string ImapCache::GetFolderUidsCachePath(const std::string &p_Folder)
{
  return GetFolderCacheDir(p_Folder) + std::string("uids");
}

std::string ImapCache::GetFolderFlagsCachePath(const std::string &p_Folder)
{
  return GetFolderCacheDir(p_Folder) + std::string("flags");
}

std::string ImapCache::GetFoldersCachePath()
{
  return GetImapCacheDir() + std::string("folders");
}

std::string ImapCache::GetMessageCachePath(const std::string &p_Folder, uint32_t p_Uid,
                                      const std::string &p_Suffix)
{
  const std::string& path = GetFolderCacheDir(p_Folder) + std::string("/") +
    std::to_string(p_Uid) + p_Suffix;
  return path;
}

std::string ImapCache::GetHeaderCachePath(const std::string &p_Folder, uint32_t p_Uid)
{
  return GetMessageCachePath(p_Folder, p_Uid, ".hdr");
}

std::string ImapCache::GetBodyCachePath(const std::string &p_Folder, uint32_t p_Uid)
{
  return GetMessageCachePath(p_Folder, p_Uid, ".eml");
}

bool ImapCache::InitFolderCacheDir(const std::string &p_Folder, int p_UidValidity)
{
  const std::string folderCacheDir = GetFolderCacheDir(p_Folder);
  return CacheUtil::CommonInitCacheDir(folderCacheDir, p_UidValidity, m_CacheEncrypt);
}

std::string ImapCache::ReadCacheFile(const std::string &p_Path)
{
  if (m_CacheEncrypt)
  {
    return Crypto::AESDecrypt(Util::ReadFile(p_Path), m_Pass);
  }
  else
  {
    return Util::ReadFile(p_Path);
  }
}

void ImapCache::WriteCacheFile(const std::string &p_Path, const std::string &p_Str)
{
  if (m_CacheEncrypt)
  {
    Util::WriteFile(p_Path, Crypto::AESEncrypt(p_Str, m_Pass));
  }
  else
  {
    Util::WriteFile(p_Path, p_Str);
  }
}

void ImapCache::DeleteCacheExceptUids(const std::string &p_Folder, const std::set<uint32_t>& p_Uids)
{
  const std::vector<std::string>& cacheFiles = Util::ListDir(GetFolderCacheDir(p_Folder));
  for (auto& cacheFile : cacheFiles)
  {
    const std::string& fileName = Util::RemoveFileExt(Util::BaseName(cacheFile));
    if (Util::IsInteger(fileName))
    {
      uint32_t uid = Util::ToInteger(fileName);
      if (p_Uids.find(uid) == p_Uids.end())
      {
        const std::string& filePath = GetFolderCacheDir(p_Folder) + cacheFile;
        Util::DeleteFile(filePath);
      }
    }
  }

  std::map<uint32_t, uint32_t> flags = Deserialize<std::map<uint32_t, uint32_t>>(ReadCacheFile(GetFolderFlagsCachePath(p_Folder)));
  for (auto flag = flags.begin(); flag != flags.end(); /* increment in loop */)
  {
    uint32_t uid = flag->first;
    if (p_Uids.find(uid) == p_Uids.end())
    {
      flag = flags.erase(flag);
    }
    else
    {
      ++flag;
    }
  }
  WriteCacheFile(GetFolderFlagsCachePath(p_Folder), Serialize(flags));
}

void ImapCache::PruneCacheFolders(const std::set<std::string>& p_KeepFolders)
{
  LOG_DEBUG_FUNC(STR(p_KeepFolders));

  if (p_KeepFolders.empty())
  {
    LOG_DEBUG("skip prune delete (empty folder list)");
    return;
  }
  
  std::set<std::string> keepPaths = { "folders", "version" };
  for (const auto& keepFolder : p_KeepFolders)
  {
    const std::string& dirName = Util::BaseName(ImapCache::GetFolderCacheDir(keepFolder));
    keepPaths.insert(dirName);
  }

  LOG_DEBUG_VAR("prune keep paths =", keepPaths);

  const std::vector<std::string>& cachePaths = Util::ListDir(ImapCache::GetImapCacheDir());
  for (const auto& cachePath : cachePaths)
  {
    if (keepPaths.find(cachePath) == keepPaths.end())
    {
      LOG_DEBUG("prune delete %s", cachePath.c_str());
      const std::string deletePath = ImapCache::GetImapCacheDir() + cachePath;
      Util::RmDir(deletePath);
    }
  }
}
