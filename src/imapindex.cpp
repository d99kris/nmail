// imapindex.cpp
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imapindex.h"

#include <unistd.h>

#include "addressbook.h"
#include "body.h"
#include "cacheutil.h"
#include "crypto.h"
#include "header.h"
#include "imapcache.h"
#include "lockfile.h"
#include "log.h"
#include "loghelp.h"
#include "maphelp.h"
#include "sethelp.h"

ImapIndex::ImapIndex(const bool p_CacheIndexEncrypt,
                     const std::string& p_Pass,
                     std::shared_ptr<ImapCache> p_ImapCache,
                     const std::function<void(const StatusUpdate&)>& p_StatusHandler)
  : m_CacheIndexEncrypt(p_CacheIndexEncrypt)
  , m_Pass(p_Pass)
  , m_ImapCache(p_ImapCache)
  , m_StatusHandler(p_StatusHandler)
{
  LOG_DEBUG_FUNC(STR(p_CacheIndexEncrypt));

  LOG_DEBUG("start thread");
  m_Running = true;
  m_Thread = std::thread(&ImapIndex::Process, this);
}

ImapIndex::~ImapIndex()
{
  LOG_DEBUG_FUNC(STR());

  LOG_DEBUG("stop thread");
  if (m_Running)
  {
    std::unique_lock<std::mutex> lock(m_ProcessMutex);
    m_Running = false;
    m_ProcessCondVar.notify_one();
  }

  if (m_Thread.joinable())
  {
    m_Thread.join();
  }
}

bool ImapIndex::ChangePass(const bool p_CacheEncrypt,
                           const std::string& p_OldPass, const std::string& p_NewPass)
{
  if (!p_CacheEncrypt) return true;

  InitCacheTempDir();
  if (!CacheUtil::DecryptCacheDir(p_OldPass, GetCacheIndexDbDir(), GetCacheIndexDbTempDir())) return false;

  Util::RmDir(GetCacheIndexDbDir());
  Util::MkDir(GetCacheIndexDbDir());
  if (!CacheUtil::EncryptCacheDir(p_NewPass, GetCacheIndexDbTempDir(), GetCacheIndexDbDir())) return false;

  CleanupCacheTempDir();

  return true;
}

void ImapIndex::NotifyIdle(bool p_IsIdle)
{
  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_IsIdle = p_IsIdle;
  if (m_IsIdle)
  {
    m_ProcessCondVar.notify_one();
  }
}

void ImapIndex::SetFolders(const std::set<std::string>& p_Folders)
{
  LOG_DEBUG_FUNC(STR(p_Folders));

  Notify notify;
  notify.m_SetFolders = p_Folders;
  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_Queue.push(notify);
  m_QueueSize = m_Queue.size();
  m_ProcessCondVar.notify_one();
}

void ImapIndex::SetUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  if (!m_SyncDone) return; // to avoid double work at first idle (sync)

  Notify notify;
  notify.m_Folder = p_Folder;
  notify.m_SetUids = p_Uids;
  m_Queue.push(notify);
  m_QueueSize = m_Queue.size();
  m_ProcessCondVar.notify_one();
}

void ImapIndex::DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  if (!m_SyncDone) return; // to avoid double work at first idle (sync)

  Notify notify;
  notify.m_Folder = p_Folder;
  notify.m_DeleteUids = p_Uids;
  m_Queue.push(notify);
  m_QueueSize = m_Queue.size();
  m_ProcessCondVar.notify_one();
}

void ImapIndex::SetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  if (!m_SyncDone) return; // to avoid double work at first idle (sync)

  Notify notify;
  notify.m_Folder = p_Folder;
  notify.m_SetBodys = p_Uids;
  m_Queue.push(notify);
  m_QueueSize = m_Queue.size();
  m_ProcessCondVar.notify_one();
}

void ImapIndex::Search(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max,
                       std::vector<Header>& p_Headers, std::vector<std::pair<std::string, uint32_t>>& p_FolderUids,
                       bool& p_HasMore)
{
  LOG_DEBUG_FUNC(STR(p_QueryStr, p_Offset, p_Max, p_HasMore));

  if (m_SearchEngine)
  {
    std::vector<std::string> docIds = m_SearchEngine->Search(p_QueryStr, p_Offset, p_Max, p_HasMore);
    for (const auto& docId : docIds)
    {
      const std::string& folder = GetFolderFromDocId(docId);
      const uint32_t uid = GetUidFromDocId(docId);

      std::map<uint32_t, Header> uidHeaders = m_ImapCache->GetHeaders(folder, std::set<uint32_t>({ uid }), false);
      if (!uidHeaders.empty())
      {
        p_Headers.push_back(uidHeaders.begin()->second);
        p_FolderUids.push_back(std::make_pair(folder, uid));
      }
    }
  }
}

void ImapIndex::Process()
{
  LOG_DEBUG("start process");

  AddressBook::Init(Util::GetAddressBookEncrypt(), m_Pass);

  InitCacheIndexDir();
  if (m_CacheIndexEncrypt)
  {
    InitCacheTempDir();
    CacheUtil::DecryptCacheDir(m_Pass, GetCacheIndexDbDir(), GetCacheIndexDbTempDir());
    m_SearchEngine.reset(new SearchEngine(GetCacheIndexDbTempDir()));
  }
  else
  {
    m_SearchEngine.reset(new SearchEngine(GetCacheIndexDbDir()));
  }

  LOG_DEBUG("entering loop");
  while (m_Running)
  {
    std::unique_lock<std::mutex> lock(m_ProcessMutex);

    while (m_Running && !(m_IsIdle && (!m_Queue.empty() || !m_SyncDone)))
    {
      ClearStatus(Status::FlagIndexing);
      m_ProcessCondVar.wait(lock);
    }

    if (!m_Running)
    {
      lock.unlock();
      break;
    }

    if (m_IsIdle && !m_SyncDone)
    {
      m_SyncDone = true;
      lock.unlock();
      HandleSyncEnqueue();
      continue;
    }

    if (m_IsIdle && !m_Queue.empty())
    {
      Notify notify = m_Queue.front();
      m_Queue.pop();
      const bool isQueueEmpty = m_Queue.empty();
      lock.unlock();

      uint32_t progress = 0;
      if (m_QueueSize > 1)
      {
        int32_t completed = (int)m_QueueSize - (int)m_Queue.size();
        if (completed > 0)
        {
          progress = (completed * 100) / m_QueueSize;
        }
      }

      SetStatus(Status::FlagIndexing, progress);

      HandleNotify(notify);
      HandleCommit(isQueueEmpty);
    }
  }

  LOG_DEBUG("exiting loop");

  HandleCommit(true);

  m_SearchEngine.reset();
  if (m_CacheIndexEncrypt && m_Dirty)
  {
    Util::RmDir(GetCacheIndexDbDir());
    Util::MkDir(GetCacheIndexDbDir());
    CacheUtil::EncryptCacheDir(m_Pass, GetCacheIndexDbTempDir(), GetCacheIndexDbDir());
    CleanupCacheTempDir();
    m_Dirty = false;
  }

  AddressBook::Cleanup();

  LOG_DEBUG("exit process");
}

void ImapIndex::HandleNotify(const Notify& p_Notify)
{
  if (!p_Notify.m_SetFolders.empty())
  {
    // Delete folders not present
    const std::vector<std::string>& docIds = m_SearchEngine->List();
    for (const auto& docId : docIds)
    {
      const std::string& folder = GetFolderFromDocId(docId);
      if (!p_Notify.m_SetFolders.count(folder))
      {
        // not found in the set of folders to keep, so remove from index
        LOG_DEBUG("remove %s", docId.c_str());
        m_SearchEngine->Remove(docId);
        m_Dirty = true;
      }
    }
  }
  else if (!p_Notify.m_SetUids.empty())
  {
    // Delete uids not present
    const std::vector<std::string>& docIds = m_SearchEngine->List();
    for (const auto& docId : docIds)
    {
      const std::string& folder = GetFolderFromDocId(docId);
      const uint32_t uid = GetUidFromDocId(docId);

      if (folder == p_Notify.m_Folder)
      {
        if (!p_Notify.m_SetUids.count(uid))
        {
          // not found in the set of uids to keep, so remove from index
          LOG_DEBUG("remove %s", docId.c_str());
          m_SearchEngine->Remove(docId);
          m_Dirty = true;
        }
      }
    }
  }
  else if (!p_Notify.m_DeleteUids.empty())
  {
    for (const auto& uid : p_Notify.m_DeleteUids)
    {
      // delete specified uid from index
      const std::string& docId = GetDocId(p_Notify.m_Folder, uid);
      LOG_DEBUG("remove %s", docId.c_str());
      m_SearchEngine->Remove(docId);
      m_Dirty = true;
    }
  }
  else if (!p_Notify.m_SetBodys.empty())
  {
    for (const auto& uid : p_Notify.m_SetBodys)
    {
      // add specified uid to index
      AddMessage(p_Notify.m_Folder, uid);
    }
  }
}

void ImapIndex::HandleCommit(bool p_ForceCommit)
{
  // commit
  static std::chrono::time_point<std::chrono::system_clock> lastCommit =
    std::chrono::system_clock::now();
  std::chrono::duration<double> secsSinceLastCommit =
    std::chrono::system_clock::now() - lastCommit;
  if (p_ForceCommit || (secsSinceLastCommit.count() >= 5.0f))
  {
    LOG_DEBUG("commit");
    m_SearchEngine->Commit();
    lastCommit = std::chrono::system_clock::now();
  }
}

void ImapIndex::HandleSyncEnqueue()
{
  LOG_DEBUG("sync enqueue start");
  std::map<std::string, std::set<uint32_t>> docFolderUids;
  const std::vector<std::string>& docIds = m_SearchEngine->List();
  for (const auto& docId : docIds)
  {
    const std::string& folder = GetFolderFromDocId(docId);
    const uint32_t uid = GetUidFromDocId(docId);
    docFolderUids[folder].insert(uid);
  }

  const std::set<std::string>& folders = m_ImapCache->GetFolders();
  for (const auto& folder : folders)
  {
    const std::set<uint32_t>& uids = m_ImapCache->GetUids(folder);
    const std::set<uint32_t>& bodyUids = MapKey(m_ImapCache->GetBodys(folder, uids, true /* p_Prefetch */));
    const std::set<uint32_t>& docUids = docFolderUids[folder];
    std::set<uint32_t> uidsToAdd = bodyUids - docUids; // present in cache, but not in index
    std::set<uint32_t> uidsToDel = docUids - bodyUids; // present in index, but not in cache

    std::unique_lock<std::mutex> lock(m_ProcessMutex);
    if (!uidsToAdd.empty())
    {
      const int maxAdd = 10;
      std::set<uint32_t> subsetUids;
      for (auto it = uidsToAdd.begin(); it != uidsToAdd.end(); ++it)
      {
        subsetUids.insert(*it);
        if ((subsetUids.size() == maxAdd) ||
            (std::next(it) == uidsToAdd.end()))
        {
          Notify notifyAdd;
          notifyAdd.m_Folder = folder;
          notifyAdd.m_SetBodys = subsetUids;
          m_Queue.push(notifyAdd);
          subsetUids.clear();
        }
      }
    }

    if (!uidsToDel.empty())
    {
      Notify notifyDel;
      notifyDel.m_Folder = folder;
      notifyDel.m_DeleteUids = uidsToDel;
      m_Queue.push(notifyDel);
    }

    m_QueueSize = m_Queue.size();
  }

  LOG_DEBUG("sync enqueue end");
}

void ImapIndex::AddMessage(const std::string& p_Folder, uint32_t p_Uid)
{
  LOG_TRACE_FUNC(STR(p_Folder, p_Uid));

  const std::string& docId = GetDocId(p_Folder, p_Uid);
  if (!m_SearchEngine->Exists(docId))
  {
    const std::map<uint32_t, Body>& uidBodys = m_ImapCache->GetBodys(p_Folder, std::set<uint32_t>({ p_Uid }), false);
    const std::map<uint32_t, Header>& uidHeaders = m_ImapCache->GetHeaders(p_Folder, std::set<uint32_t>(
                                                                             { p_Uid }), false);

    if (!uidBodys.empty() && !uidHeaders.empty())
    {
      const Header& header = uidHeaders.begin()->second;
      const Body& body = uidBodys.begin()->second;

      const int64_t timeStamp = header.GetTimeStamp();
      const std::string& bodyText = body.GetTextPlain();
      const std::string& subject = header.GetSubject();
      const std::string& from = header.GetFrom();
      const std::string& to = header.GetTo() + " " + header.GetCc() + " " + header.GetBcc();

      LOG_DEBUG("add %s", docId.c_str());
      m_SearchEngine->Index(docId, timeStamp, bodyText, subject, from, to);
      m_Dirty = true;

      // @todo: decouple addressbook population from cache index
      AddressBook::Add(header.GetUniqueId(), header.GetAddresses());
    }
  }
}

std::string ImapIndex::GetDocId(const std::string& p_Folder, const uint32_t p_Uid)
{
  return p_Folder + "_" + std::to_string(p_Uid);
}

std::string ImapIndex::GetFolderFromDocId(const std::string& p_DocId)
{
  const std::size_t lastUnderscorePos = p_DocId.find_last_of("_");
  if (lastUnderscorePos != std::string::npos)
  {
    return p_DocId.substr(0, lastUnderscorePos);
  }
  else
  {
    return "";
  }
}

uint32_t ImapIndex::GetUidFromDocId(const std::string& p_DocId)
{
  const std::size_t lastUnderscorePos = p_DocId.find_last_of("_");
  if (lastUnderscorePos != std::string::npos)
  {
    const std::string& uidStr = p_DocId.substr(lastUnderscorePos + 1);
    return static_cast<uint32_t>(std::stoul(uidStr));
  }
  else
  {
    return 0;
  }
}

std::string ImapIndex::GetCacheIndexDir()
{
  return CacheUtil::GetCacheDir() + std::string("searchindex/");
}

std::string ImapIndex::GetCacheIndexDbDir()
{
  return CacheUtil::GetCacheDir() + std::string("searchindex/db/");
}

std::string ImapIndex::GetCacheIndexDbTempDir()
{
  return Util::GetTempDir() + std::string("searchindexdb/");
}

void ImapIndex::InitCacheIndexDir()
{
  static const int version = 7; // note: keep synchronized with AddressBook (for now)
  const std::string cacheDir = GetCacheIndexDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, m_CacheIndexEncrypt);
  Util::MkDir(GetCacheIndexDbDir());
}

void ImapIndex::InitCacheTempDir()
{
  Util::RmDir(GetCacheIndexDbTempDir());
  Util::MkDir(GetCacheIndexDbTempDir());
}

void ImapIndex::CleanupCacheTempDir()
{
  Util::RmDir(GetCacheIndexDbTempDir());
}

void ImapIndex::SetStatus(uint32_t p_Flags, int32_t p_Progress /* = -1 */)
{
  StatusUpdate statusUpdate;
  statusUpdate.SetFlags = p_Flags;
  statusUpdate.Progress = p_Progress;
  if (m_StatusHandler)
  {
    m_StatusHandler(statusUpdate);
  }
}

void ImapIndex::ClearStatus(uint32_t p_Flags)
{
  StatusUpdate statusUpdate;
  statusUpdate.ClearFlags = p_Flags;
  if (m_StatusHandler)
  {
    m_StatusHandler(statusUpdate);
  }
}
