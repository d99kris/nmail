// imapcache.cpp
//
// Copyright (c) 2020-2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.
//
// @todo: consider moving heavy set operations to worker thread

#include "imapcache.h"

#include "body.h"
#include "cacheutil.h"
#include "crypto.h"
#include "flag.h"
#include "header.h"
#include "lockfile.h"
#include "loghelp.h"
#include "maphelp.h"
#include "util.h"
#include "serialization.h"
#include "sethelp.h"
#include "sqlitehelp.h"

struct ImapCache::DbConnection
{
  DbConnection(const std::string& p_DbPath)
    : m_DbPath(p_DbPath)
  {
    OpenDb();
  }

  void CloseDb()
  {
    m_Database.reset();
  }

  void OpenDb()
  {
    m_Database.reset(new sqlite::database(m_DbPath));
    *m_Database << "PRAGMA synchronous = OFF";
    *m_Database << "PRAGMA journal_mode = MEMORY";
  }

  std::shared_ptr<sqlite::database> m_Database;
  std::string m_DbPath;
  bool m_Dirty = false;
};

ImapCache::ImapCache(const bool p_CacheEncrypt, const std::string& p_Pass)
  : m_CacheEncrypt(p_CacheEncrypt)
  , m_Pass(p_Pass)
{
  InitHeadersCache();
  InitBodysCache();
  InitUidFlagsCache();
  InitValidityCache();

  m_Folders = GetFolders();
}

ImapCache::~ImapCache()
{
  CleanupHeadersCache();
  CleanupBodysCache();
  CleanupUidFlagsCache();
  CleanupValidityCache();
}

bool ImapCache::ChangePass(const bool p_CacheEncrypt,
                           const std::string& p_OldPass, const std::string& p_NewPass)
{
  if (!p_CacheEncrypt) return true;

  std::string headersDir = GetCacheDbDir(HeadersDb);
  std::vector<std::string> headerFiles = Util::ListDir(headersDir);
  for (const auto& headerFile : headerFiles)
  {
    std::string path = headersDir + headerFile;
    std::string tmpPath = path + ".tmp";
    if (!Crypto::AESDecryptFile(path, tmpPath, p_OldPass)) return false;

    if (!Crypto::AESEncryptFile(tmpPath, path, p_NewPass)) return false;

    Util::DeleteFile(tmpPath);

    std::cout << ".";
  }

  std::string bodysDir = GetCacheDbDir(BodysDb);
  std::vector<std::string> bodyFiles = Util::ListDir(bodysDir);
  for (const auto& bodyFile : bodyFiles)
  {
    std::string path = bodysDir + bodyFile;
    std::string tmpPath = path + ".tmp";
    if (!Crypto::AESDecryptFile(path, tmpPath, p_OldPass)) return false;

    if (!Crypto::AESEncryptFile(tmpPath, path, p_NewPass)) return false;

    Util::DeleteFile(tmpPath);

    std::cout << ".";
  }

  std::string uidFlagsDir = GetCacheDbDir(UidFlagsDb);
  std::vector<std::string> uidFlagFiles = Util::ListDir(uidFlagsDir);
  for (const auto& uidFlagFile : uidFlagFiles)
  {
    std::string path = uidFlagsDir + uidFlagFile;
    std::string tmpPath = path + ".tmp";
    if (!Crypto::AESDecryptFile(path, tmpPath, p_OldPass)) return false;

    if (!Crypto::AESEncryptFile(tmpPath, path, p_NewPass)) return false;

    Util::DeleteFile(tmpPath);

    std::cout << ".";
  }

  std::string validityDir = GetCacheDbDir(ValidityDb);
  std::vector<std::string> validityFiles = Util::ListDir(validityDir);
  for (const auto& validityFile : validityFiles)
  {
    std::string path = validityDir + validityFile;
    std::string tmpPath = path + ".tmp";
    if (!Crypto::AESDecryptFile(path, tmpPath, p_OldPass)) return false;

    if (!Crypto::AESEncryptFile(tmpPath, path, p_NewPass)) return false;

    Util::DeleteFile(tmpPath);

    std::cout << ".";
  }

  std::string path = GetHeadersFoldersPath();
  std::string data = Crypto::AESDecrypt(Util::ReadFile(path), p_OldPass);
  Util::WriteFile(path, Crypto::AESEncrypt(data, p_NewPass));

  std::cout << "\n";
  return true;
}

// get all folders
std::set<std::string> ImapCache::GetFolders()
{
  LOG_DURATION();
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  return Serialization::FromString<std::set<std::string>>(ReadCacheFile(GetHeadersFoldersPath()));
}

// set all folders
void ImapCache::SetFolders(const std::set<std::string>& p_Folders)
{
  LOG_DURATION();
  if (p_Folders.empty()) return; // sanity check

  std::set<std::string> deletedFolders;
  {
    std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
    deletedFolders = m_Folders - p_Folders;
    WriteCacheFile(GetHeadersFoldersPath(), Serialization::ToString(p_Folders));
  }

  for (const auto& deletedFolder : deletedFolders)
  {
    // @todo: consider closing db and deleting file-system files instead
    ClearFolder(deletedFolder);
  }
}

// get all uids
std::set<uint32_t> ImapCache::GetUids(const std::string& p_Folder)
{
  LOG_DURATION();
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, false /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::set<uint32_t> uids;
  try
  {
    auto lambda = [&](const std::vector<uint32_t>& data)
    {
      uids = ToSet(data);
    };

    *db << "SELECT uids.uids FROM uids LIMIT 1" >> lambda;
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  return uids;
}

// set all uids
void ImapCache::SetUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DURATION();

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);

  std::string delUidList;

  try
  {
    std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, true /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;

    std::set<uint32_t> oldUids;
    auto lambda = [&](const std::vector<uint32_t>& data)
    {
      oldUids = ToSet(data);
    };

    *db << "SELECT uids.uids FROM uids LIMIT 1" >> lambda;

    if (p_Uids != oldUids)
    {
      *db << "begin;";
      *db << "DELETE FROM uids;";
      *db << "INSERT INTO uids (uids) VALUES (?);" << ToVector(p_Uids);

      std::set<uint32_t> delUids = oldUids - p_Uids;
      if (!delUids.empty())
      {
        std::stringstream sstream;
        std::copy(delUids.begin(), delUids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
        delUidList = sstream.str();
        delUidList.pop_back(); // assumes non-empty input set

        *db << "DELETE FROM flags WHERE uid IN (" + delUidList + ");";
      }

      *db << "commit;";
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  if (!delUidList.empty())
  {
    try
    {
      std::shared_ptr<DbConnection> dbCon = GetDb(BodysDb, p_Folder, true /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      *db << "DELETE FROM bodys WHERE uid IN (" + delUidList + ");";
    }
    catch (const sqlite::sqlite_exception& ex)
    {
      HANDLE_SQLITE_EXCEPTION(ex);
    }

    try
    {
      std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      *db << "DELETE FROM headers WHERE uid IN (" + delUidList + ");";
    }
    catch (const sqlite::sqlite_exception& ex)
    {
      HANDLE_SQLITE_EXCEPTION(ex);
    }
  }
}

// get specified headers
std::map<uint32_t, Header> ImapCache::GetHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                                                 const bool p_Prefetch)
{
  LOG_DURATION();
  std::map<uint32_t, Header> headers;
  if (p_Uids.empty()) return headers;

  std::map<uint32_t, Header> updateCacheHeaders;

  try
  {
    std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
    std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, false /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;

    std::stringstream sstream;
    std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
    std::string uidlist = sstream.str();
    uidlist.pop_back(); // assumes non-empty input set

    if (!p_Prefetch)
    {
      auto lambda = [&](const uint32_t& uid, const std::vector<char>& data)
      {
        Header header;
        header = Serialization::FromBytes<Header>(data);
        if (header.ParseIfNeeded())
        {
          updateCacheHeaders[uid] = header;
        }

        if (header.GetTimeStamp() != 0)
        {
          headers.insert(std::make_pair(uid, header));
        }
        else
        {
          LOG_WARNING("invalid cached header folder %s uid = %d",
                      p_Folder.c_str(), uid);
        }
      };

      *db << "SELECT uid, data FROM headers WHERE uid IN (" + uidlist + ");" >> lambda;
    }
    else
    {
      auto lambda = [&](const uint32_t& uid)
      {
        headers.insert(std::make_pair(uid, Header()));
      };

      *db << "SELECT uid FROM headers WHERE uid IN (" + uidlist + ");" >> lambda;
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  if (!updateCacheHeaders.empty())
  {
    SetHeaders(p_Folder, updateCacheHeaders);
  }

  return headers;
}

// set specified headers
void ImapCache::SetHeaders(const std::string& p_Folder, const std::map<uint32_t, Header>& p_Headers)
{
  LOG_DURATION();
  if (p_Headers.empty()) return;

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  try
  {
    *db << "begin;";
    for (const auto& header : p_Headers)
    {
      const uint32_t uid = header.first;
      *db << "INSERT OR REPLACE INTO headers (uid, data) VALUES (?, ?);" << uid <<
        Serialization::ToBytes(header.second);
    }
    *db << "commit;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// get specified flags
std::map<uint32_t, uint32_t> ImapCache::GetFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DURATION();
  std::map<uint32_t, uint32_t> flags;
  if (p_Uids.empty()) return flags;

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, false /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  try
  {
    auto lambda = [&](const uint32_t& uid, const uint32_t& flag)
    {
      flags.insert(std::make_pair(uid, flag));
    };

    *db << "SELECT uid, flag FROM flags WHERE uid IN (" + uidlist + ");" >> lambda;
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  return flags;
}

// set specified flags
void ImapCache::SetFlags(const std::string& p_Folder, const std::map<uint32_t, uint32_t>& p_Flags)
{
  LOG_DURATION();
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  try
  {
    *db << "begin;";
    for (const auto& flag : p_Flags)
    {
      *db << "INSERT OR REPLACE INTO flags (uid, flag) VALUES (?, ?);" << flag.first << flag.second;
    }
    *db << "commit;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// get specified bodys
std::map<uint32_t, Body> ImapCache::GetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                                             const bool p_Prefetch)
{
  LOG_DURATION();
  std::map<uint32_t, Body> bodys;
  if (p_Uids.empty()) return bodys;

  std::map<uint32_t, Body> updateCacheBodys;

  try
  {
    std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
    std::shared_ptr<DbConnection> dbCon = GetDb(BodysDb, p_Folder, false /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;

    std::stringstream sstream;
    std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
    std::string uidlist = sstream.str();
    uidlist.pop_back(); // assumes non-empty input set

    if (!p_Prefetch)
    {
      auto lambda = [&](const uint32_t& uid, const std::vector<char>& data)
      {
        Body body;
        body = Serialization::FromBytes<Body>(data);
        if (body.ParseIfNeeded())
        {
          updateCacheBodys[uid] = body;
        }
        bodys.insert(std::make_pair(uid, body));
      };

      *db << "SELECT uid, data FROM bodys WHERE uid IN (" + uidlist + ");" >> lambda;
    }
    else
    {
      auto lambda = [&](const uint32_t& uid)
      {
        bodys.insert(std::make_pair(uid, Body()));
      };

      *db << "SELECT uid FROM bodys WHERE uid IN (" + uidlist + ");" >> lambda;
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  if (!updateCacheBodys.empty())
  {
    SetBodys(p_Folder, updateCacheBodys);
  }

  return bodys;
}

// set specified bodys
void ImapCache::SetBodys(const std::string& p_Folder, const std::map<uint32_t, Body>& p_Bodys)
{
  LOG_DURATION();
  if (p_Bodys.empty()) return;

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(BodysDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  try
  {
    *db << "begin;";
    for (const auto& body : p_Bodys)
    {
      *db << "INSERT OR REPLACE INTO bodys (uid, data) VALUES (?, ?);" << body.first <<
        Serialization::ToBytes(body.second);
    }
    *db << "commit;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// checks cached uid validity and clears existing cache if invalid
bool ImapCache::CheckUidValidity(const std::string& p_Folder, int p_Uid)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uid));
  bool rv = true;
  try
  {
    std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
    int storedUid = -1;

    const std::string commonFolder = "common";
    const std::string dbFolder = Util::ToHex(p_Folder);
    if (storedUid == -1)
    {
      std::shared_ptr<DbConnection> dbCon = GetDb(ValidityDb, commonFolder,
                                                  false /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      auto lambda = [&](const uint32_t& uid)
      {
        storedUid = uid;
      };

      *db << "SELECT validity.uid FROM validity WHERE folder = '" + dbFolder + "'"
        >> lambda;
    }

    // @todo: remove below when we update version in InitUidFlagsCache().
    bool isLegacy = false;
    if (storedUid == -1)
    {
      std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder,
                                                  false /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      auto lambda = [&](const std::vector<int32_t>& vecdata)
      {
        if (vecdata.size() == 1)
        {
          storedUid = vecdata.at(0);
        }
      };

      *db << "SELECT uidvalidity.uidvalidity FROM uidvalidity LIMIT 1" >> lambda;
      isLegacy = (storedUid != -1);
    }

    if (isLegacy || (p_Uid != storedUid))
    {
      LOG_DEBUG("folder %s uidvalidity %d", p_Folder.c_str(), p_Uid);

      std::shared_ptr<DbConnection> dbCon = GetDb(ValidityDb, commonFolder,
                                                  true /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      *db << "INSERT OR REPLACE INTO validity (folder, uid) VALUES (?, ?);"
          << dbFolder << p_Uid;

      if (storedUid != -1)
      {
        if (p_Uid == storedUid) // isLegacy
        {
          LOG_INFO("folder %s uidvalidity migrated", p_Folder.c_str());
        }
        else
        {
          LOG_INFO("folder %s uidvalidity updated", p_Folder.c_str());
        }
      }
      else
      {
        LOG_DEBUG("folder %s uidvalidity created", p_Folder.c_str());
      }
    }

    rv = (p_Uid == storedUid);
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  if (!rv)
  {
    ClearFolder(p_Folder);
  }

  return rv;
}

// set specified uids seen flag
void ImapCache::SetFlagSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, const bool p_Value)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_Value));

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  try
  {
    *db << "UPDATE flags SET flag = ? WHERE uid IN (" + uidlist + ");" << (uint32_t)p_Value;
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// clear specified folder
void ImapCache::ClearFolder(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);

  try
  {
    std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;
    *db << "DELETE FROM headers;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  try
  {
    std::shared_ptr<DbConnection> dbCon = GetDb(BodysDb, p_Folder, true /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;
    *db << "DELETE FROM bodys;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  try
  {
    std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, true /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;
    *db << "DELETE FROM uids;";
    *db << "DELETE FROM flags;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// delete specified messages
void ImapCache::DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  DeleteUids(p_Folder, p_Uids);
  DeleteFlags(p_Folder, p_Uids);
  DeleteHeaders(p_Folder, p_Uids);
  DeleteBodys(p_Folder, p_Uids);
}

// delete specified uids
void ImapCache::DeleteUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  try
  {
    std::set<uint32_t> uids;
    auto lambda = [&](const std::vector<uint32_t>& data)
    {
      uids = ToSet(data);
    };

    *db << "SELECT uids.uids FROM uids LIMIT 1" >> lambda;

    for (auto& uid : p_Uids)
    {
      uids.erase(uid);
    }

    *db << "begin;";
    *db << "DELETE FROM uids;";
    *db << "INSERT INTO uids (uids) VALUES (?);" << ToVector(uids);
    *db << "commit;";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// delete specified flags
void ImapCache::DeleteFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(UidFlagsDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  try
  {
    *db << "DELETE FROM flags WHERE uid IN (" + uidlist + ");";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// delete specified headers
void ImapCache::DeleteHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  try
  {
    *db << "DELETE FROM headers WHERE uid IN (" + uidlist + ");";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// delete specified bodys
void ImapCache::DeleteBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(BodysDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  try
  {
    *db << "DELETE FROM bodys WHERE uid IN (" + uidlist + ");";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

bool ImapCache::Export(const std::string& p_Path)
{
  // @todo: determine what is correct/portable MailDir format
  Util::MkDir(p_Path);
  Util::MkDir(p_Path + "/new");
  Util::MkDir(p_Path + "/tmp");
  Util::MkDir(p_Path + "/cur");

  const std::set<std::string> folders = GetFolders();
  for (const auto& folder : folders)
  {
    std::string folderName = folder;
    Util::ReplaceString(folderName, "/", "_");
    std::string folderPath = p_Path + "/" + folderName;
    Util::MkDir(folderPath);
    Util::MkDir(folderPath + "/new");
    Util::MkDir(folderPath + "/tmp");
    Util::MkDir(folderPath + "/cur");

    const std::set<uint32_t> uids = GetUids(folder);
    if (uids.empty()) continue;

    const std::map<uint32_t, Body> bodys = GetBodys(folder, uids, false /*p_Prefetch*/);

    for (const auto& body : bodys)
    {
      const uint32_t uid = body.first;
      const std::string& data = body.second.GetData();
      const std::string path = folderPath + "/cur/" + std::to_string(uid) + ".eml";
      Util::WriteFile(path, data);
    }
  }
  return true;
}

void ImapCache::InitHeadersCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  static const int version = 2;
  CacheUtil::CommonInitCacheDir(GetCacheDir(HeadersDb), version, m_CacheEncrypt);
  Util::MkDir(GetCacheDbDir(HeadersDb));
  if (m_CacheEncrypt)
  {
    Util::RmDir(GetTempDbDir(HeadersDb));
    Util::MkDir(GetTempDbDir(HeadersDb));
  }
}

void ImapCache::CleanupHeadersCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  CloseDbs(HeadersDb);
}

void ImapCache::InitBodysCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  static const int version = 2;
  CacheUtil::CommonInitCacheDir(GetCacheDir(BodysDb), version, m_CacheEncrypt);
  Util::MkDir(GetCacheDbDir(BodysDb));
  if (m_CacheEncrypt)
  {
    Util::RmDir(GetTempDbDir(BodysDb));
    Util::MkDir(GetTempDbDir(BodysDb));
  }
}

void ImapCache::CleanupBodysCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  CloseDbs(BodysDb);
}

void ImapCache::InitUidFlagsCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  static const int version = 2;
  CacheUtil::CommonInitCacheDir(GetCacheDir(UidFlagsDb), version, m_CacheEncrypt);
  Util::MkDir(GetCacheDbDir(UidFlagsDb));
  if (m_CacheEncrypt)
  {
    Util::RmDir(GetTempDbDir(UidFlagsDb));
    Util::MkDir(GetTempDbDir(UidFlagsDb));
  }
}

void ImapCache::CleanupUidFlagsCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  CloseDbs(UidFlagsDb);
}

void ImapCache::InitValidityCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  static const int version = 1;
  CacheUtil::CommonInitCacheDir(GetCacheDir(ValidityDb), version, m_CacheEncrypt);
  Util::MkDir(GetCacheDbDir(ValidityDb));
  if (m_CacheEncrypt)
  {
    Util::RmDir(GetTempDbDir(ValidityDb));
    Util::MkDir(GetTempDbDir(ValidityDb));
  }
}

void ImapCache::CleanupValidityCache()
{
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  CloseDbs(ValidityDb);
}

std::string ImapCache::GetDbTypeName(ImapCache::DbType p_DbType)
{
  static const std::map<DbType, std::string> dbTypeNames =
  {
    { HeadersDb, "headers" },
    { BodysDb, "messages" },
    { UidFlagsDb, "uidflags" },
    { ValidityDb, "validity" },
  };
  return dbTypeNames.at(p_DbType);
}

std::string ImapCache::GetCacheDir(ImapCache::DbType p_DbType)
{
  return CacheUtil::GetCacheDir() + GetDbTypeName(p_DbType) + std::string("/");
}

std::string ImapCache::GetCacheDbDir(ImapCache::DbType p_DbType)
{
  return CacheUtil::GetCacheDir() + GetDbTypeName(p_DbType) + std::string("/db/");
}

std::string ImapCache::GetTempDbDir(ImapCache::DbType p_DbType)
{
  return Util::GetTempDir() + GetDbTypeName(p_DbType) + std::string("/");
}

std::string ImapCache::GetHeadersFoldersPath()
{
  return GetCacheDir(HeadersDb) + std::string("folders");
}

std::string ImapCache::GetDbName(const std::string& p_Folder)
{
  return (m_CacheEncrypt ? Crypto::SHA256(p_Folder) : Util::ToHex(p_Folder)) + ".sqlite";
}

std::string ImapCache::GetDbPath(ImapCache::DbType p_DbType, const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(GetDbTypeName(p_DbType), p_Folder));

  const std::string& dbName = GetDbName(p_Folder);
  std::string dbPath;
  if (m_CacheEncrypt)
  {
    dbPath = GetTempDbDir(p_DbType) + dbName;
    std::string cacheDbPath = GetCacheDbDir(p_DbType) + dbName;
    if (!Util::Exists(dbPath))
    {
      if (Util::Exists(cacheDbPath))
      {
        if (!Crypto::AESDecryptFile(cacheDbPath, dbPath, m_Pass))
        {
          Util::DeleteFile(dbPath);
        }
      }
    }
  }
  else
  {
    dbPath = GetCacheDbDir(p_DbType) + dbName;
  }

  return dbPath;
}

void ImapCache::WriteDb(ImapCache::DbType p_DbType, const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(GetDbTypeName(p_DbType), p_Folder));

  if (m_CacheEncrypt) // just in case
  {
    const std::string& dbName = GetDbName(p_Folder);
    std::string dbPath = GetTempDbDir(p_DbType) + dbName;
    std::string cacheDbPath = GetCacheDbDir(p_DbType) + dbName;
    if (!Crypto::AESEncryptFile(dbPath, cacheDbPath, m_Pass))
    {
      Util::DeleteFile(cacheDbPath);
    }
  }
}

void ImapCache::CreateDb(ImapCache::DbType p_DbType, const std::string& p_DbPath)
{
  LOG_DEBUG_FUNC(STR(GetDbTypeName(p_DbType), p_DbPath));

  try
  {
    sqlite::database db(p_DbPath);
    if (p_DbType == HeadersDb)
    {
      db << "CREATE TABLE IF NOT EXISTS headers (uid INT, data BLOB, PRIMARY KEY (uid));";
    }
    else if (p_DbType == BodysDb)
    {
      db << "CREATE TABLE IF NOT EXISTS bodys (uid INT, data BLOB, PRIMARY KEY (uid));";
    }
    else if (p_DbType == UidFlagsDb)
    {
      db << "CREATE TABLE IF NOT EXISTS uids (uids BLOB);";
      // @todo: remove uidvalidity creation on update of version in InitUidFlagsCache
      db << "CREATE TABLE IF NOT EXISTS uidvalidity (uidvalidity BLOB);";
      db << "CREATE TABLE IF NOT EXISTS flags (uid INT, flag INT, PRIMARY KEY (uid));";
    }
    else if (p_DbType == ValidityDb)
    {
      db << "CREATE TABLE IF NOT EXISTS validity (folder TEXT, uid INT, PRIMARY KEY (folder));";
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

// must be called with cachelock
std::shared_ptr<ImapCache::DbConnection> ImapCache::GetDb(ImapCache::DbType p_DbType, const std::string& p_Folder,
                                                          bool p_Writable)
{
  std::shared_ptr<ImapCache::DbConnection> dbConnection;
  auto& dbMap = m_DbConnections[p_DbType];
  auto it = dbMap.find(p_Folder);
  if (it != dbMap.end())
  {
    // use existing open connection
    dbConnection = it->second;
  }
  else
  {
    // open new connection
    const std::string& dbPath = GetDbPath(p_DbType, p_Folder);
    if (!Util::Exists(dbPath))
    {
      CreateDb(p_DbType, dbPath);
    }

    dbConnection = std::shared_ptr<DbConnection>(new DbConnection(dbPath));
    dbMap[p_Folder] = dbConnection;
  }

  if (m_CacheEncrypt)
  {
    // for encrypted db - only keep one writable db to minimize shutdown time
    auto& currentWriteDb = m_CurrentWriteDb[p_DbType];
    if (p_Writable && (currentWriteDb != p_Folder))
    {
      if (!currentWriteDb.empty())
      {
        std::shared_ptr<ImapCache::DbConnection> prevDbConnection = dbMap.at(currentWriteDb);
        if (prevDbConnection->m_Dirty)
        {
          prevDbConnection->CloseDb();
          WriteDb(p_DbType, currentWriteDb);
          prevDbConnection->OpenDb();
          prevDbConnection->m_Dirty = false;
        }
      }

      currentWriteDb = p_Folder;
    }
  }

  dbConnection->m_Dirty |= p_Writable;

  return dbConnection;
}

// must be called with cachelock
void ImapCache::CloseDbs(ImapCache::DbType p_DbType)
{
  LOG_DEBUG_FUNC(STR(GetDbTypeName(p_DbType)));

  auto& dbMap = m_DbConnections[p_DbType];
  if (m_CacheEncrypt)
  {
    // for encrypted mode, write back any dirty db (should only be one)
    for (auto it = dbMap.begin(); it != dbMap.end(); ++it)
    {
      const std::string& folder = it->first;
      std::shared_ptr<ImapCache::DbConnection> dbConnection = it->second;
      if (dbConnection->m_Dirty)
      {
        dbConnection->CloseDb();
        WriteDb(p_DbType, folder);
        dbConnection->m_Dirty = false;
      }
    }
  }

  dbMap.clear();
}

std::string ImapCache::ReadCacheFile(const std::string& p_Path)
{
  return m_CacheEncrypt ? Crypto::AESDecrypt(Util::ReadFile(p_Path), m_Pass) : Util::ReadFile(p_Path);
}

void ImapCache::WriteCacheFile(const std::string& p_Path, const std::string& p_Str)
{
  Util::WriteFile(p_Path, m_CacheEncrypt ? Crypto::AESEncrypt(p_Str, m_Pass) : p_Str);
}
