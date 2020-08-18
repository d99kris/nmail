// addressbook.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "addressbook.h"

#include <map>
#include <string>
#include <utility>

#include <sqlite_modern_cpp.h>

#include "cacheutil.h"
#include "crypto.h"
#include "log.h"
#include "loghelp.h"
#include "maphelp.h"
#include "serialized.h"
#include "util.h"

std::mutex AddressBook::m_Mutex;
bool AddressBook::m_AddressBookEncrypt = true;
std::string AddressBook::m_Pass;
std::unique_ptr<sqlite::database> AddressBook::m_Db;

void AddressBook::Init(const bool p_AddressBookEncrypt, const std::string& p_Pass)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_AddressBookEncrypt = p_AddressBookEncrypt;
  m_Pass = p_Pass;

  InitCacheDir();

  if (m_AddressBookEncrypt)
  {
    Util::RmDir(GetAddressBookTempDir());
    Util::MkDir(GetAddressBookTempDir());
    CacheUtil::DecryptCacheDir(m_Pass, GetAddressBookCacheDir(), GetAddressBookTempDir());
    const std::string& dbPath = GetAddressBookTempDir() + "addresses.sqlite";
    m_Db.reset(new sqlite::database(dbPath));
  }
  else
  {
    const std::string& dbPath = GetAddressBookCacheDir() + "addresses.sqlite";
    m_Db.reset(new sqlite::database(dbPath));
  }

  if (!m_Db) return;

  *m_Db << "CREATE TABLE IF NOT EXISTS msgids (msgid TEXT PRIMARY KEY NOT NULL);";
  *m_Db << "CREATE TABLE IF NOT EXISTS addresses (address TEXT PRIMARY KEY NOT NULL, usages INT);";
}

void AddressBook::Cleanup()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db) return;

  if (m_AddressBookEncrypt)
  {
    Util::RmDir(GetAddressBookCacheDir());
    Util::MkDir(GetAddressBookCacheDir());
    CacheUtil::EncryptCacheDir(m_Pass, GetAddressBookTempDir(), GetAddressBookCacheDir());
    m_Db.reset();
  }
  else
  {
    m_Db.reset();
  }
}

void AddressBook::Add(const std::string& p_MsgId, const std::set<std::string>& p_Addresses)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Db) return;

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
  }
}

std::vector<std::string> AddressBook::Get(const std::string& p_Filter)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  
  if (!m_Db) return std::vector<std::string>();

  std::vector<std::string> addresses;
  if (p_Filter.empty())
  {
    *m_Db << "SELECT address FROM addresses ORDER BY usages DESC;" >>
      [&](const std::string& address)
      {
        addresses.push_back(address);
      };
  }
  else
  {
    // @todo: strip out any % from p_Filter?
    *m_Db << "SELECT address FROM addresses WHERE address LIKE ? ORDER BY usages DESC;" << ("%" + p_Filter + "%") >>
      [&](const std::string& address)
      {
        addresses.push_back(address);
      };
  }

  return addresses;
}

void AddressBook::InitCacheDir()
{
  static const int version = 3; // note: keep synchronized with ImapIndex (for now)
  const std::string cacheDir = GetAddressBookCacheDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, m_AddressBookEncrypt);
}

std::string AddressBook::GetAddressBookCacheDir()
{
  return Util::GetApplicationDir() + std::string("cache/") + std::string("address/");
}

std::string AddressBook::GetAddressBookTempDir()
{
  return Util::GetTempDir() + std::string("address/");
}
