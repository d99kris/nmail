// addressbook.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "addressbook.h"

#include <map>
#include <string>
#include <utility>

#include <sqlite_modern_cpp.h>

#include "apputil.h"
#include "cacheutil.h"
#include "crypto.h"
#include "fileutil.h"
#include "log.h"
#include "loghelp.h"
#include "maphelp.h"
#include "sqlitehelp.h"
#include "strutil.h"

std::mutex AddressBook::m_Mutex;
std::string AddressBook::m_DbPath;
bool AddressBook::m_AddressBookEncrypt = true;
std::string AddressBook::m_Pass;
std::unique_ptr<sqlite::database> AddressBook::m_Db;
bool AddressBook::m_Dirty = false;

void AddressBook::Init(const bool p_AddressBookEncrypt, const std::string& p_Pass)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_AddressBookEncrypt = p_AddressBookEncrypt;
  m_Pass = p_Pass;

  InitCacheDir();

  if (m_AddressBookEncrypt)
  {
    FileUtil::RmDir(GetAddressBookTempDbDir());
    FileUtil::MkDir(GetAddressBookTempDbDir());
    CacheUtil::DecryptCacheDir(m_Pass, GetAddressBookCacheDbDir(), GetAddressBookTempDbDir());
    m_DbPath = GetAddressBookTempDbDir() + "addresses.sqlite";
  }
  else
  {
    m_DbPath = GetAddressBookCacheDbDir() + "addresses.sqlite";
  }

  OpenDb();

  if (!m_Db) return;

  try
  {
    int64_t tableCount = 0;
    *m_Db << "SELECT COUNT(*) FROM sqlite_master;" >> tableCount;
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    const int code = ex.get_code();
    if (AppUtil::GetReadOnly() || ((code != SQLITE_CORRUPT) && (code != SQLITE_NOTADB)))
    {
      HANDLE_SQLITE_EXCEPTION(ex);
    }

    // keep corrupt db aside and start over with an empty one
    const std::string corruptDbPath = m_DbPath + ".corrupt";
    LOG_WARNING("addressbook db corrupt (%d: \"%s\"), renaming to %s", code, ex.what(), corruptDbPath.c_str());
    CloseDb();
    rename(m_DbPath.c_str(), corruptDbPath.c_str());
    FileUtil::DeleteFile(m_DbPath + "-journal");
    OpenDb();
  }

  if (!m_Db) return;

  try
  {
    *m_Db << "CREATE TABLE IF NOT EXISTS msgids (msgid TEXT PRIMARY KEY NOT NULL);";
    *m_Db << "CREATE TABLE IF NOT EXISTS addresses (address TEXT PRIMARY KEY NOT NULL, usages INT);";
    *m_Db << "CREATE TABLE IF NOT EXISTS fromaddresses (address TEXT PRIMARY KEY NOT NULL, usages INT);";
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

void AddressBook::Cleanup()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db) return;

  CloseDb();

  if (m_AddressBookEncrypt && m_Dirty)
  {
    if (!AppUtil::GetReadOnly())
    {
      FileUtil::RmDir(GetAddressBookCacheDbDir());
      FileUtil::MkDir(GetAddressBookCacheDbDir());
      CacheUtil::EncryptCacheDir(m_Pass, GetAddressBookTempDbDir(), GetAddressBookCacheDbDir());
      m_Dirty = false;
    }
    else
    {
      LOG_WARNING("attempted to write addressbook in read-only mode");
    }
  }
}

void AddressBook::OpenDb()
{
  if (AppUtil::GetReadOnly())
  {
    sqlite::sqlite_config config;
    config.flags = sqlite::OpenFlags::READONLY;
    m_Db.reset(new sqlite::database(m_DbPath, config));
  }
  else
  {
    m_Db.reset(new sqlite::database(m_DbPath));
  }

  // custom function used by address filter, as sqlite's built-in like / lower() only handle ascii
  m_Db->define("nmfold", [](std::string p_Str) -> std::string
  {
    return StrUtil::ToFold(p_Str);
  });
}

void AddressBook::CloseDb()
{
  m_Db.reset();
}

bool AddressBook::ChangePass(const bool p_CacheEncrypt,
                             const std::string& p_OldPass, const std::string& p_NewPass)
{
  if (!p_CacheEncrypt) return true;

  FileUtil::RmDir(GetAddressBookTempDbDir());
  FileUtil::MkDir(GetAddressBookTempDbDir());
  if (!CacheUtil::DecryptCacheDir(p_OldPass, GetAddressBookCacheDbDir(), GetAddressBookTempDbDir())) return false;

  FileUtil::RmDir(GetAddressBookCacheDbDir());
  FileUtil::MkDir(GetAddressBookCacheDbDir());
  if (!CacheUtil::EncryptCacheDir(p_NewPass, GetAddressBookTempDbDir(), GetAddressBookCacheDbDir())) return false;

  return true;
}

void AddressBook::Add(const std::string& p_MsgId, const std::set<std::string>& p_Addresses)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db || AppUtil::GetReadOnly()) return;

  try
  {
    // check if msgid already processed
    int msgidExists = 0;
    *m_Db << "SELECT COUNT(msgid) FROM msgids WHERE msgid=?;" << p_MsgId >> msgidExists;
    if (msgidExists)
    {
      LOG_TRACE("skip already processed msgid %s", p_MsgId.c_str());
      return;
    }
    else
    {
      LOG_TRACE("add msgid %s", p_MsgId.c_str());
      *m_Db << "INSERT INTO msgids (msgid) VALUES (?);" << p_MsgId;
    }

    for (const auto& address : p_Addresses)
    {
      int addressExists = 0;
      *m_Db << "SELECT COUNT(usages) FROM addresses WHERE address=?;" << address >> addressExists;
      if (addressExists == 0)
      {
        // add address
        LOG_TRACE("add address %s", address.c_str());
        *m_Db << "INSERT INTO addresses (address, usages) VALUES (?, 1);" << address;
      }
      else
      {
        // increment address usage
        LOG_TRACE("increment address %s", address.c_str());
        *m_Db << "UPDATE addresses SET usages = usages + 1 WHERE address = ?;" << address;
      }

      m_Dirty = true;
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

void AddressBook::AddFrom(const std::string& p_Address)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db || AppUtil::GetReadOnly()) return;

  try
  {
    int addressExists = 0;
    *m_Db << "SELECT COUNT(usages) FROM fromaddresses WHERE address=?;" << p_Address >> addressExists;
    if (addressExists == 0)
    {
      // add address
      LOG_TRACE("add fromaddress %s", p_Address.c_str());
      *m_Db << "INSERT INTO fromaddresses (address, usages) VALUES (?, 1);" << p_Address;
    }
    else
    {
      // increment address usage
      LOG_TRACE("increment fromaddress %s", p_Address.c_str());
      *m_Db << "UPDATE fromaddresses SET usages = usages + 1 WHERE address = ?;" << p_Address;
    }

    m_Dirty = true;
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }
}

std::vector<std::string> AddressBook::Get(const std::string& p_Filter)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db) return std::vector<std::string>();

  std::vector<std::string> addresses;
  try
  {
    if (p_Filter.empty())
    {
      *m_Db << "SELECT address FROM addresses ORDER BY usages DESC;" >>
        [&](const std::string& address) { addresses.push_back(address); };
    }
    else
    {
      *m_Db << "SELECT address FROM addresses WHERE instr(nmfold(address), nmfold(?)) > 0 ORDER BY usages DESC;" <<
        p_Filter >>
        [&](const std::string& address) { addresses.push_back(address); };
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  return addresses;
}

std::vector<std::string> AddressBook::GetFrom(const std::string& p_Filter)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db) return std::vector<std::string>();

  std::vector<std::string> addresses;
  try
  {
    if (p_Filter.empty())
    {
      *m_Db << "SELECT address FROM fromaddresses ORDER BY usages DESC;" >>
        [&](const std::string& address) { addresses.push_back(address); };
    }
    else
    {
      *m_Db << "SELECT address FROM fromaddresses WHERE instr(nmfold(address), nmfold(?)) > 0 "
        "ORDER BY usages DESC;" << p_Filter >>
        [&](const std::string& address) { addresses.push_back(address); };
    }
  }
  catch (const sqlite::sqlite_exception& ex)
  {
    HANDLE_SQLITE_EXCEPTION(ex);
  }

  return addresses;
}

void AddressBook::InitCacheDir()
{
  static const int version = 8; // note: keep synchronized with ImapIndex (for now)
  const std::string cacheDir = GetAddressBookCacheDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, m_AddressBookEncrypt);
  FileUtil::MkDir(GetAddressBookCacheDbDir());
}

std::string AddressBook::GetAddressBookCacheDir()
{
  return CacheUtil::GetCacheDir() + std::string("addressbook/");
}

std::string AddressBook::GetAddressBookCacheDbDir()
{
  return CacheUtil::GetCacheDir() + std::string("addressbook/db/");
}

std::string AddressBook::GetAddressBookTempDbDir()
{
  return FileUtil::GetTempDir() + std::string("addressbookdb/");
}
