// imapcache.cpp
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

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

  m_Folders = GetFolders();
}

ImapCache::~ImapCache()
{
  CleanupHeadersCache();
  CleanupBodysCache();
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
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, false /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::set<uint32_t> uids;
  *db << "SELECT uids.uids FROM uids LIMIT 1" >>
    [&](const std::vector<uint32_t>& data)
    {
      uids = ToSet(data);
    };

  return uids;
}

// set all uids
void ImapCache::SetUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DURATION();
  if (p_Uids.empty()) return;

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  *db << "begin;";
  *db << "DELETE FROM uids;";
  *db << "INSERT INTO uids (uids) VALUES (?);" << ToVector(p_Uids);
  *db << "commit;";
}

// get specified headers
std::map<uint32_t, Header> ImapCache::GetHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                                                 const bool p_Prefetch)
{
  LOG_DURATION();
  std::map<uint32_t, Header> headers;
  if (p_Uids.empty()) return headers;

  std::map<uint32_t, Header> updateCacheHeaders;

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
      *db << "SELECT uid, data FROM headers WHERE uid IN (" + uidlist + ");" >>
        [&](const uint32_t& uid, const std::vector<char>& data)
        {
          Header header;
          header = Serialization::FromBytes<Header>(data);
          if (header.ParseIfNeeded())
          {
            updateCacheHeaders[uid] = header;
          }
          headers.insert(std::make_pair(uid, header));
        };
    }
    else
    {
      *db << "SELECT uid FROM headers WHERE uid IN (" + uidlist + ");" >>
        [&](const uint32_t& uid)
        {
          headers.insert(std::make_pair(uid, Header()));
        };
    }
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

  *db << "begin;";
  for (const auto& header : p_Headers)
  {
    const uint32_t uid = header.first;
    *db << "INSERT OR REPLACE INTO headers (uid, data) VALUES (?, ?);" << uid << Serialization::ToBytes(header.second);
  }
  *db << "commit;";
}

// get specified flags
std::map<uint32_t, uint32_t> ImapCache::GetFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DURATION();
  std::map<uint32_t, uint32_t> flags;
  if (p_Uids.empty()) return flags;

  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, false /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  *db << "SELECT uid, flag FROM flags WHERE uid IN (" + uidlist + ");" >>
    [&](const uint32_t& uid, const uint32_t& flag)
    {
      flags.insert(std::make_pair(uid, flag));
    };

  return flags;
}

// set specified flags
void ImapCache::SetFlags(const std::string& p_Folder, const std::map<uint32_t, uint32_t>& p_Flags)
{
  LOG_DURATION();
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  *db << "begin;";
  for (const auto& flag : p_Flags)
  {
    *db << "INSERT OR REPLACE INTO flags (uid, flag) VALUES (?, ?);" << flag.first << flag.second;
  }
  *db << "commit;";
}

// get specified bodys
std::map<uint32_t, Body> ImapCache::GetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                                             const bool p_Prefetch)
{
  LOG_DURATION();
  std::map<uint32_t, Body> bodys;
  if (p_Uids.empty()) return bodys;

  std::map<uint32_t, Body> updateCacheBodys;

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
      *db << "SELECT uid, data FROM bodys WHERE uid IN (" + uidlist + ");" >>
        [&](const uint32_t& uid, const std::vector<char>& data)
        {
          Body body;
          body = Serialization::FromBytes<Body>(data);
          if (body.ParseIfNeeded())
          {
            updateCacheBodys[uid] = body;
          }
          bodys.insert(std::make_pair(uid, body));
        };
    }
    else
    {
      *db << "SELECT uid FROM bodys WHERE uid IN (" + uidlist + ");" >>
        [&](const uint32_t& uid)
        {
          bodys.insert(std::make_pair(uid, Body()));
        };
    }
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

  *db << "begin;";
  for (const auto& body : p_Bodys)
  {
    *db << "INSERT OR REPLACE INTO bodys (uid, data) VALUES (?, ?);" << body.first <<
    Serialization::ToBytes(body.second);
  }
  *db << "commit;";
}

// checks cached uid validity and clears existing cache if invalid
bool ImapCache::CheckUidValidity(const std::string& p_Folder, int p_UidValidity)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_UidValidity));
  bool rv = true;
  {
    std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
    int storedUidValidity = -1;

    {
      std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, false /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      *db << "SELECT uidvalidity.uidvalidity FROM uidvalidity LIMIT 1" >>
        [&](const std::vector<int32_t>& vecdata)
        {
          if (vecdata.size() == 1)
          {
            storedUidValidity = vecdata.at(0);
          }
        };
    }

    if (p_UidValidity != storedUidValidity)
    {
      LOG_DEBUG("folder %s uidvalidity %d", p_Folder.c_str(), p_UidValidity);

      std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
      std::shared_ptr<sqlite::database> db = dbCon->m_Database;

      const std::vector<int32_t> vecdata = { p_UidValidity };
      *db << "begin;";
      *db << "DELETE FROM uidvalidity;";
      *db << "INSERT INTO uidvalidity (uidvalidity) VALUES (?);" << vecdata;
      *db << "commit;";

      rv = false;
    }
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
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  *db << "UPDATE flags SET flag = ? WHERE uid IN (" + uidlist + ");" << (uint32_t)p_Value;
}

// clear specified folder
void ImapCache::ClearFolder(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  {
    std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;

    *db << "DELETE FROM uids;";
    *db << "DELETE FROM flags;";
    *db << "DELETE FROM headers;";
  }

  {
    std::shared_ptr<DbConnection> dbCon = GetDb(BodysDb, p_Folder, true /* p_Writable */);
    std::shared_ptr<sqlite::database> db = dbCon->m_Database;

    *db << "DELETE FROM bodys;";
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
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::set<uint32_t> uids;
  *db << "SELECT uids.uids FROM uids LIMIT 1" >>
    [&](const std::vector<uint32_t>& data)
    {
      uids = ToSet(data);
    };

  for (auto& uid : p_Uids)
  {
    uids.erase(uid);
  }

  *db << "begin;";
  *db << "DELETE FROM uids;";
  *db << "INSERT INTO uids (uids) VALUES (?);" << ToVector(uids);
  *db << "commit;";
}

// delete specified flags
void ImapCache::DeleteFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));
  std::lock_guard<std::mutex> cacheLock(m_CacheMutex);
  std::shared_ptr<DbConnection> dbCon = GetDb(HeadersDb, p_Folder, true /* p_Writable */);
  std::shared_ptr<sqlite::database> db = dbCon->m_Database;

  std::stringstream sstream;
  std::copy(p_Uids.begin(), p_Uids.end(), std::ostream_iterator<uint32_t>(sstream, ","));
  std::string uidlist = sstream.str();
  uidlist.pop_back(); // assumes non-empty input set

  *db << "DELETE FROM flags WHERE uid IN (" + uidlist + ");";
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

  *db << "DELETE FROM headers WHERE uid IN (" + uidlist + ");";
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

  *db << "DELETE FROM bodys WHERE uid IN (" + uidlist + ");";
}

bool ImapCache::Export(const std::string& p_Path)
{
  // @todo: determine what is correct/portable MailDir format and implement export for it
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
  static const int version = 1;
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
  static const int version = 1;
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

std::string ImapCache::GetDbTypeName(ImapCache::DbType p_DbType)
{
  static const std::map<DbType, std::string> dbTypeNames =
  {
    { HeadersDb, "headers" },
    { BodysDb, "messages" },
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
        Crypto::AESDecryptFile(cacheDbPath, dbPath, m_Pass);
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
    Crypto::AESEncryptFile(dbPath, cacheDbPath, m_Pass);
  }
}

void ImapCache::CreateDb(ImapCache::DbType p_DbType, const std::string& p_DbPath)
{
  LOG_DEBUG_FUNC(STR(GetDbTypeName(p_DbType), p_DbPath));

  sqlite::database db(p_DbPath);
  if (p_DbType == HeadersDb)
  {
    db << "CREATE TABLE IF NOT EXISTS uids (uids BLOB);";
    db << "CREATE TABLE IF NOT EXISTS flags (uid INT, flag INT, PRIMARY KEY (uid));";
    db << "CREATE TABLE IF NOT EXISTS headers (uid INT, data BLOB, PRIMARY KEY (uid));";
    db << "CREATE TABLE IF NOT EXISTS uidvalidity (uidvalidity BLOB);";
  }
  else if (p_DbType == BodysDb)
  {
    db << "CREATE TABLE IF NOT EXISTS bodys (uid INT, data BLOB, PRIMARY KEY (uid));";
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
