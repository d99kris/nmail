// uimodel.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "owned_mutex.h"
#include "ui.h"

class UiModel
{
private:
  // data guarded by the model mutex, reached through Lock only
  class Impl
  {
  public:
    void InvalidateUidCache(const std::string& p_Folder);
    std::map<std::string, uint32_t>& GetDisplayUids(const std::string& p_Folder);
    std::set<uint32_t>& GetHeaderUids(const std::string& p_Folder);
    void UpdateDisplayUids(const std::string& p_Folder,
                           const std::set<uint32_t>& p_RemovedUids = std::set<uint32_t>(),
                           const std::set<uint32_t>& p_AddedUids = std::set<uint32_t>(),
                           bool p_FilterUpdated = false);

  private:
    std::string GetDisplayUidsKey(const std::string& p_Folder, uint32_t p_Uid, Ui::SortFilter p_SortFilter);

  public:
    std::string m_TrashFolder;
    std::string m_SpamFolder;
    std::string m_DraftsFolder;
    std::string m_SentFolder;
    std::map<std::string, std::string> m_DetectedSpecialFolders;

    Status m_Status;
    std::set<std::string> m_Folders;
    std::map<std::string, std::set<uint32_t>> m_Uids;
    std::map<std::string, std::map<uint32_t, Header>> m_Headers;
    std::map<std::string, std::map<uint32_t, uint32_t>> m_Flags;
    std::map<std::string, std::map<uint32_t, Body>> m_Bodys;
    std::map<std::string, Ui::SortFilter> m_SortFilter;
    std::map<std::string, std::set<uint32_t>> m_HeaderUids;
    std::map<std::string, std::map<Ui::SortFilter, std::map<std::string, uint32_t>>> m_DisplayUids;
    std::map<std::string, std::map<Ui::SortFilter, uint64_t>> m_DisplayUidsVersion;
    std::map<std::string, uint64_t> m_HeaderUidsVersion;

    Ui::SyncState m_SyncState = Ui::SyncStateIdle;
    bool m_HasRequestedFolders = false;
    bool m_HasPrefetchRequestedFolders = false;
    std::map<std::string, bool> m_HasRequestedUids;
    std::map<std::string, std::set<uint32_t>> m_PrefetchedHeaders;
    std::map<std::string, std::set<uint32_t>> m_RequestedHeaders;

    std::map<std::string, std::set<uint32_t>> m_PrefetchedFlags;
    std::map<std::string, std::set<uint32_t>> m_RequestedFlags;

    std::map<std::string, std::set<uint32_t>> m_PrefetchedBodys;
    std::map<std::string, std::set<uint32_t>> m_RequestedBodys;

    std::string m_FilterCustomStr;

    bool m_MessageListSearchHasMore = false;
    std::vector<Header> m_MessageListSearchResultHeaders;
    std::vector<std::pair<std::string, uint32_t>> m_MessageListSearchResultFolderUids;

    std::deque<SmtpManager::Result> m_SmtpErrorResults;

    // set when the message list cursor needs a resync, performed by ui thread
    bool m_UpdateIndexFromUid = false;
    bool m_UpdateUidFromIndex = false;
  };

public:
  // holds the model mutex while in scope
  class Lock
  {
  public:
    explicit Lock(UiModel* p_Model)
      : m_Lock(p_Model->m_Mutex)
      , m_Impl(p_Model->m_Impl)
    {
    }

    Impl* operator-> ()
    {
      return &m_Impl;
    }

  private:
    std::lock_guard<owned_mutex> m_Lock;
    Impl& m_Impl;
  };

  UiModel(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_DefaultFrom,
          uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders,
          const std::function<void(char)>& p_UiRequestHandler,
          const std::function<void(const std::string&, bool)>& p_DialogMessageHandler);
  virtual ~UiModel();

  void ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response);
  void ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result);
  void SmtpResultHandler(const SmtpManager::Result& p_Result);
  void StatusHandler(const StatusUpdate& p_StatusUpdate);
  void SearchHandler(const ImapManager::SearchQuery& p_SearchQuery,
                     const ImapManager::SearchResult& p_SearchResult);

  void HandleSpecialFolders();
  void SetTrashFolder(const std::string& p_TrashFolder);
  void SetSpamFolder(const std::string& p_SpamFolder);
  void SetDraftsFolder(const std::string& p_DraftsFolder);
  void SetSentFolder(const std::string& p_SentFolder);
  std::string GetTrashFolder();
  std::string GetSpamFolder();
  std::string GetDraftsFolder();
  std::string GetSentFolder();
  void SetShowProgress(int p_ShowProgress);
  void SetHasRequestedUids(const std::string& p_Folder, bool p_HasRequestedUids);
  void MoveMessages(const std::set<uint32_t>& p_Uids, const std::string& p_From,
                    const std::string& p_To);
  void DeleteMessages(const std::set<uint32_t>& p_Uids, const std::string& p_Folder);
  void SetSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Seen);
  void MarkSeen(const std::string& p_Folder, uint32_t p_Uid);
  void InvalidateUiCache(const std::string& p_Folder);
  void SetImapManager(std::shared_ptr<ImapManager> p_ImapManager);
  void ResetImapManager();
  std::shared_ptr<ImapManager> GetImapManager();
  void SetSpecialFoldersHandler(const std::function<void(const std::map<std::string, std::string>&)>& p_Handler);
  void SetClientStoreSent(bool p_ClientStoreSent);
  void SetNewMsgBell(bool p_NewMsgBell);
  std::string GetInbox();
  std::string GetAddress();
  uint32_t GetPrefetchLevel();
  bool GetPrefetchAllHeaders();

private:
  void AsyncUiRequest(char p_UiRequest);
  void SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn = false);

private:
  std::function<void(char)> m_UiRequestHandler;
  std::function<void(const std::string&, bool)> m_DialogMessageHandler;

  Impl m_Impl;
  owned_mutex m_Mutex;

  std::shared_ptr<ImapManager> m_ImapManager;
  std::function<void(const std::map<std::string, std::string>&)> m_SpecialFoldersHandler;
  bool m_ClientStoreSent = false;

  std::string m_Inbox;
  std::string m_Address;
  std::string m_DefaultFrom;
  uint32_t m_PrefetchLevel = 0;

  bool m_NewMsgBell = false;

  bool m_PrefetchAllHeaders = true;
};
