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

  const std::set<std::string>& folders = m_ImapCache->GetFolders();
  for (const auto& folder : folders)
  {
    EnqueueAddFolder(folder);
  }

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

void ImapIndex::NotifyIdle(bool p_IsIdle)
{
  // @todo: set idle when in offline mode to enable indexing
  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_IsIdle = p_IsIdle;
  if (m_IsIdle)
  {
    m_ProcessCondVar.notify_one();
  }
}

void ImapIndex::EnqueueSyncFolders(std::set<std::string>& p_Folders)
{
  LOG_DEBUG_FUNC(STR(p_Folders));

  std::set<std::string> deleteFolders = m_AddedFolders - p_Folders;
  std::set<std::string> addFolders = p_Folders - m_AddedFolders;

  for (const auto& deleteFolder : deleteFolders)
  {
    EnqueueDeleteFolder(deleteFolder);
  }

  for (const auto& addFolder : addFolders)
  {
    EnqueueAddFolder(addFolder);
  }
}

void ImapIndex::EnqueueAddFolder(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  if (m_AddedFolders.find(p_Folder) == m_AddedFolders.end())
  {
    m_AddedFolders.insert(p_Folder);
    m_AddQueue[p_Folder].insert(0); // use uid 0 to indicate all uids
    m_ProcessCondVar.notify_one();
  }

  m_QueueSize = m_AddQueue.size() + m_DeleteQueue.size();
}

void ImapIndex::EnqueueDeleteFolder(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_DeleteQueue[p_Folder].clear();
  m_DeleteQueue[p_Folder].insert(0); // use uid 0 to indicate keep no uids
  m_AddedFolders.erase(p_Folder);
  m_AddQueue.erase(p_Folder);
  m_ProcessCondVar.notify_one();

  m_QueueSize = m_AddQueue.size() + m_DeleteQueue.size();
}

void ImapIndex::EnqueueAddMessage(const std::string& p_Folder, uint32_t p_Uid)
{
  LOG_TRACE_FUNC(STR(p_Folder, p_Uid));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_AddQueue[p_Folder].insert(p_Uid);
  m_ProcessCondVar.notify_one();
}

void ImapIndex::EnqueueDeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_KeepUids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_KeepUids));

  std::unique_lock<std::mutex> lock(m_ProcessMutex);
  m_DeleteQueue[p_Folder] = p_KeepUids;
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
    std::string addFolder;
    int32_t addUid = 0;
    std::string deleteFolder;
    std::set<uint32_t> keepUids;
    bool performCommit = false;

    {
      std::unique_lock<std::mutex> lock(m_ProcessMutex);
      while (true)
      {
        if (!m_Running) break;

        if (m_IsIdle && (!m_AddQueue.empty() || !m_DeleteQueue.empty())) break;

        ClearStatus(Status::FlagIndexing);

        m_ProcessCondVar.wait(lock);
      }

      if (!m_Running)
      {
        break;
      }

      const std::set<std::string>& addFolders = MapKey<std::string>(m_AddQueue);
      const std::set<std::string>& deleteFolders = MapKey<std::string>(m_DeleteQueue);

      if (!deleteFolders.empty()) // delete
      {
        deleteFolder = *deleteFolders.begin();
        keepUids = m_DeleteQueue[deleteFolder];
        m_DeleteQueue.erase(deleteFolder);
        performCommit = true;
      }
      else if (!addFolders.empty()) // add
      {
        addFolder = *addFolders.begin();
        auto firstIt = m_AddQueue[addFolder].begin();
        addUid = *firstIt;
        m_AddQueue[addFolder].erase(firstIt);
        if (m_AddQueue[addFolder].empty())
        {
          m_AddQueue.erase(addFolder);
          performCommit = true;
        }
      }

    }

    uint32_t progress = 0;
    if (m_QueueSize > 1)
    {
      int32_t completed = (int)m_QueueSize - ((int)m_AddQueue.size() + (int)m_DeleteQueue.size());
      if (completed > 0)
      {
        progress = (completed * 100) / m_QueueSize;
      }
    }

    SetStatus(Status::FlagIndexing, progress);
    if (!deleteFolder.empty())
    {
      if ((keepUids.size() == 1) && (*keepUids.begin() == 0))
      {
        DeleteFolder(deleteFolder);
      }
      else
      {
        DeleteMessages(deleteFolder, keepUids);
      }
    }
    else if (!addFolder.empty())
    {
      if (addUid == 0)
      {
        AddFolder(addFolder);
      }
      else
      {
        AddMessage(addFolder, addUid);
      }
    }
    else
    {
      LOG_WARNING("unexpected state");
    }

    // commit
    static std::chrono::time_point<std::chrono::system_clock> lastCommit =
      std::chrono::system_clock::now();
    std::chrono::duration<double> secsSinceLastCommit =
      std::chrono::system_clock::now() - lastCommit;
    if (performCommit || (secsSinceLastCommit.count() >= 5.0f))
    {
      LOG_DEBUG("commit");
      m_SearchEngine->Commit();
      lastCommit = std::chrono::system_clock::now();
    }

  }

  LOG_DEBUG("exiting loop");

  m_SearchEngine.reset();
  if (m_CacheIndexEncrypt && m_Dirty)
  {
    Util::RmDir(GetCacheIndexDbDir());
    Util::MkDir(GetCacheIndexDbDir());
    CacheUtil::EncryptCacheDir(m_Pass, GetCacheIndexDbTempDir(), GetCacheIndexDbDir());
    CleanupCacheTempDir();
    m_Dirty = false;
  }

  LOG_DEBUG("exit process");
}

void ImapIndex::AddFolder(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));

  const std::set<uint32_t>& uids = m_ImapCache->GetUids(p_Folder);
  const std::map<uint32_t, Body>& uidBodys = m_ImapCache->GetBodys(p_Folder, uids, true);
  for (const auto& uidBody : uidBodys)
  {
    const uint32_t uid = uidBody.first;
    const std::string& docId = GetDocId(p_Folder, uid);

    if (!m_SearchEngine->Exists(docId))
    {
      EnqueueAddMessage(p_Folder, uid);
    }
  }
}

void ImapIndex::DeleteFolder(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));

  const std::vector<std::string>& docIds = m_SearchEngine->List();
  for (const auto& docId : docIds)
  {
    const std::string& folder = GetFolderFromDocId(docId);
    if (folder == p_Folder)
    {
      m_SearchEngine->Remove(docId);
      m_Dirty = true;
    }
  }
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

      m_SearchEngine->Index(docId, timeStamp, bodyText, subject, from, to);
      m_Dirty = true;

      // @todo: decouple addressbook population from cache index
      AddressBook::Add(header.GetUniqueId(), header.GetAddresses());
    }
  }
}

void ImapIndex::DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_KeepUids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_KeepUids));

  const std::vector<std::string>& docIds = m_SearchEngine->List();
  for (const auto& docId : docIds)
  {
    const std::string& folder = GetFolderFromDocId(docId);
    const uint32_t uid = GetUidFromDocId(docId);

    if (folder == p_Folder)
    {
      if (p_KeepUids.find(uid) == p_KeepUids.end())
      {
        // not found in the set of uids to keep, so remove from index
        m_SearchEngine->Remove(docId);
        m_Dirty = true;
      }
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
  static const int version = 6; // note: keep synchronized with AddressBook (for now)
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
