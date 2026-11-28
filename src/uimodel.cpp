// uimodel.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uimodel.h"

#include <algorithm>
#include <cstdint>

#include "addressbook.h"
#include "apputil.h"
#include "fileutil.h"
#include "flag.h"
#include "loghelp.h"
#include "mailutil.h"
#include "maphelp.h"
#include "offlinequeue.h"
#include "sethelp.h"
#include "status.h"
#include "strutil.h"

UiModel::UiModel(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_DefaultFrom,
                 uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders,
                 const std::function<void(char)>& p_UiRequestHandler,
                 const std::function<void(const std::string&, bool)>& p_DialogMessageHandler)
  : m_UiRequestHandler(p_UiRequestHandler)
  , m_DialogMessageHandler(p_DialogMessageHandler)
  , m_Inbox(p_Inbox)
  , m_Address(p_Address)
  , m_DefaultFrom(p_DefaultFrom)
  , m_PrefetchLevel(p_PrefetchLevel)
  , m_PrefetchAllHeaders(p_PrefetchAllHeaders)
{
}

UiModel::~UiModel()
{
}

void UiModel::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  if (!Ui::IsRunning()) return;

  char uiRequest = Ui::UiRequestNone;

  if (p_Request.m_PrefetchLevel < Ui::PrefetchLevelFullSync)
  {
    std::set<uint32_t> fetchHeaderUids;
    std::set<uint32_t> fetchFlagUids;

    {
      Lock model(this);

      if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
      {
        model->m_Folders = p_Response.m_Folders;
        uiRequest |= Ui::UiRequestDrawAll;
        LOG_DEBUG_VAR("new folders =", p_Response.m_Folders);

        if (!p_Response.m_SpecialFolders.empty())
        {
          model->m_DetectedSpecialFolders = p_Response.m_SpecialFolders;
          uiRequest |= Ui::UiRequestSpecialFolders;
        }
      }

      if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
      {
        if (p_Response.m_UidInvalid)
        {
          LOG_INFO("folder %s uid invalid", p_Response.m_Folder.c_str());
          model->InvalidateUidCache(p_Response.m_Folder);
        }

        const std::set<uint32_t> newUids = p_Response.m_Uids - model->m_Uids[p_Response.m_Folder];
        if (!p_Response.m_Cached && (p_Response.m_Folder == m_Inbox) && !newUids.empty())
        {
          if (m_NewMsgBell)
          {
            LOG_DEBUG("bell");
            uiRequest |= Ui::UiRequestBell;
          }
        }

        const std::set<uint32_t>& removedUids = model->m_Uids[p_Response.m_Folder] - p_Response.m_Uids;
        if (!removedUids.empty())
        {
          LOG_DEBUG_VAR("del uids =", removedUids);
          model->UpdateDisplayUids(p_Response.m_Folder, removedUids);
          model->m_Headers[p_Response.m_Folder] = model->m_Headers[p_Response.m_Folder] - removedUids;
        }

        model->m_Uids[p_Response.m_Folder] = p_Response.m_Uids;
        uiRequest |= Ui::UiRequestDrawAll;
        model->m_UpdateIndexFromUid = true;
        LOG_DEBUG_VAR("new uids =", newUids);

        if (!m_PrefetchAllHeaders && !newUids.empty())
        {
          model->UpdateDisplayUids(p_Response.m_Folder, std::set<uint32_t>(), newUids);
        }

        if (m_PrefetchAllHeaders)
        {
          std::map<uint32_t, Header>& headers = model->m_Headers[p_Response.m_Folder];
          std::map<uint32_t, uint32_t>& flags = model->m_Flags[p_Response.m_Folder];
          std::set<uint32_t>& requestedHeaders = model->m_RequestedHeaders[p_Response.m_Folder];
          std::set<uint32_t>& requestedFlags = model->m_RequestedFlags[p_Response.m_Folder];
          for (auto& uid : newUids)
          {
            if ((headers.find(uid) == headers.end()) &&
                (requestedHeaders.find(uid) == requestedHeaders.end()))
            {
              fetchHeaderUids.insert(uid);
              requestedHeaders.insert(uid);
            }
          }

          for (auto& uid : p_Response.m_Uids)
          {
            if (((flags.find(uid) == flags.end()) &&
                 (requestedFlags.find(uid) == requestedFlags.end())))
            {
              fetchFlagUids.insert(uid);
              requestedFlags.insert(uid);
            }
          }
        }
      }

      if (!p_Request.m_GetHeaders.empty() &&
          !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed))
      {
        const std::map<uint32_t, Header>& headers = p_Response.m_Headers;

        model->m_Headers[p_Response.m_Folder].insert(headers.begin(), headers.end());
        if (m_PrefetchAllHeaders)
        {
          model->UpdateDisplayUids(p_Response.m_Folder, std::set<uint32_t>(), MapKey(headers));
        }
        uiRequest |= Ui::UiRequestDrawAll;
        model->m_UpdateIndexFromUid = true;
        LOG_DEBUG_VAR("new headers =", MapKey(headers));
      }

      if (!p_Request.m_GetFlags.empty() &&
          !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed))
      {
        std::map<uint32_t, uint32_t> newFlags = p_Response.m_Flags;
        newFlags.insert(model->m_Flags[p_Response.m_Folder].begin(), model->m_Flags[p_Response.m_Folder].end());
        model->m_Flags[p_Response.m_Folder] = newFlags;
        uiRequest |= Ui::UiRequestDrawAll;
        LOG_DEBUG_VAR("new flags =", MapKey(p_Response.m_Flags));
      }

      if (!p_Request.m_GetBodys.empty() &&
          !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed))
      {
        model->m_Bodys[p_Response.m_Folder].insert(p_Response.m_Bodys.begin(), p_Response.m_Bodys.end());
        uiRequest |= Ui::UiRequestDrawAll;
        LOG_DEBUG_VAR("new bodys =", MapKey(p_Response.m_Bodys));
      }
    }

    // perform fetch
    if (!fetchHeaderUids.empty())
    {
      const int maxHeadersFetchRequest = 25;
      std::set<uint32_t> subsetFetchHeaderUids;
      for (auto it = fetchHeaderUids.begin(); it != fetchHeaderUids.end(); ++it)
      {
        subsetFetchHeaderUids.insert(*it);
        if ((subsetFetchHeaderUids.size() == maxHeadersFetchRequest) ||
            (std::next(it) == fetchHeaderUids.end()))
        {
          ImapManager::Request request;
          request.m_Folder = p_Response.m_Folder;
          request.m_GetHeaders = subsetFetchHeaderUids;

          LOG_DEBUG_VAR("async req headers =", subsetFetchHeaderUids);
          m_ImapManager->AsyncRequest(request);

          subsetFetchHeaderUids.clear();
        }
      }
    }

    if (!fetchFlagUids.empty())
    {
      const int maxFlagsFetchRequest = 1000;
      std::set<uint32_t> subsetFetchFlagUids;
      for (auto it = fetchFlagUids.begin(); it != fetchFlagUids.end(); ++it)
      {
        subsetFetchFlagUids.insert(*it);
        if ((subsetFetchFlagUids.size() == maxFlagsFetchRequest) ||
            (std::next(it) == fetchFlagUids.end()))
        {
          ImapManager::Request request;
          request.m_Folder = p_Response.m_Folder;
          request.m_GetFlags = subsetFetchFlagUids;

          LOG_DEBUG_VAR("async req flags =", subsetFetchFlagUids);
          m_ImapManager->AsyncRequest(request);

          subsetFetchFlagUids.clear();
        }
      }
    }
  }

  if (p_Request.m_PrefetchLevel == Ui::PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      Lock model(this);
      for (auto& folder : p_Response.m_Folders)
      {
        if (!Ui::IsRunning())
        {
          break;
        }

        // Allow re-fetching folder uids, to enable full sync more than once per session
        {
          ImapManager::Request request;
          request.m_PrefetchLevel = Ui::PrefetchLevelFullSync;
          request.m_Folder = folder;
          request.m_GetUids = true;
          LOG_DEBUG_VAR("prefetch req uids =", folder);
          m_ImapManager->PrefetchRequest(request);
        }
      }
    }

    if (p_Request.m_GetUids &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      const std::string& folder = p_Response.m_Folder;

      std::set<uint32_t> prefetchHeaders;
      std::set<uint32_t> prefetchFlags;
      std::set<uint32_t> prefetchBodys;

      {
        Lock model(this);

        if (p_Response.m_UidInvalid)
        {
          LOG_INFO("folder %s uid invalid", p_Response.m_Folder.c_str());
          model->InvalidateUidCache(p_Response.m_Folder);
        }

        std::map<uint32_t, Header>& headers = model->m_Headers[folder];
        std::set<uint32_t>& requestedHeaders = model->m_RequestedHeaders[folder];
        std::set<uint32_t>& prefetchedHeaders = model->m_PrefetchedHeaders[folder];

        std::map<uint32_t, uint32_t>& flags = model->m_Flags[folder];
        std::set<uint32_t>& requestedFlags = model->m_RequestedFlags[folder];
        std::set<uint32_t>& prefetchedFlags = model->m_PrefetchedFlags[folder];

        std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
        std::set<uint32_t>& requestedBodys = model->m_RequestedBodys[folder];
        std::set<uint32_t>& prefetchedBodys = model->m_PrefetchedBodys[folder];

        for (auto& uid : p_Response.m_Uids)
        {
          if ((headers.find(uid) == headers.end()) &&
              (requestedHeaders.find(uid) == requestedHeaders.end()) &&
              (prefetchedHeaders.find(uid) == prefetchedHeaders.end()))
          {
            prefetchHeaders.insert(uid);
            prefetchedHeaders.insert(uid);
          }

          if ((flags.find(uid) == flags.end()) &&
              (requestedFlags.find(uid) == requestedFlags.end()) &&
              (prefetchedFlags.find(uid) == prefetchedFlags.end()))
          {
            prefetchFlags.insert(uid);
            prefetchedFlags.insert(uid);
          }

          if ((bodys.find(uid) == bodys.end()) &&
              (requestedBodys.find(uid) == requestedBodys.end()) &&
              (prefetchedBodys.find(uid) == prefetchedBodys.end()))
          {
            prefetchBodys.insert(uid);
            prefetchedBodys.insert(uid);
          }
        }
      }

      const int maxHeadersFetchRequest = 25;
      if (!prefetchHeaders.empty())
      {
        std::set<uint32_t> subsetPrefetchHeaders;
        for (auto it = prefetchHeaders.begin(); it != prefetchHeaders.end(); ++it)
        {
          if (!Ui::IsRunning()) break;

          subsetPrefetchHeaders.insert(*it);
          if ((subsetPrefetchHeaders.size() == maxHeadersFetchRequest) ||
              (std::next(it) == prefetchHeaders.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = Ui::PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetHeaders = subsetPrefetchHeaders;

            LOG_DEBUG_VAR("prefetch req headers =", subsetPrefetchHeaders);
            m_ImapManager->PrefetchRequest(request);

            subsetPrefetchHeaders.clear();
          }
        }
      }

      const int maxFlagsFetchRequest = 1000;
      if (!prefetchFlags.empty())
      {
        std::set<uint32_t> subsetPrefetchFlags;
        for (auto it = prefetchFlags.begin(); it != prefetchFlags.end(); ++it)
        {
          if (!Ui::IsRunning()) break;

          subsetPrefetchFlags.insert(*it);
          if ((subsetPrefetchFlags.size() == maxFlagsFetchRequest) ||
              (std::next(it) == prefetchFlags.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = Ui::PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetFlags = subsetPrefetchFlags;

            LOG_DEBUG_VAR("prefetch req flags =", subsetPrefetchFlags);
            m_ImapManager->PrefetchRequest(request);

            subsetPrefetchFlags.clear();
          }
        }
      }

      const int maxBodysFetchRequest = 1;
      if (!prefetchBodys.empty())
      {
        std::set<uint32_t> subsetPrefetchBodys;
        for (auto it = prefetchBodys.begin(); it != prefetchBodys.end(); ++it)
        {
          if (!Ui::IsRunning()) break;

          subsetPrefetchBodys.insert(*it);
          if ((subsetPrefetchBodys.size() == maxBodysFetchRequest) ||
              (std::next(it) == prefetchBodys.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = Ui::PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetBodys = subsetPrefetchBodys;

            LOG_DEBUG_VAR("prefetch req bodys =", subsetPrefetchBodys);
            m_ImapManager->PrefetchRequest(request);

            subsetPrefetchBodys.clear();
          }
        }
      }
    }
  }

  if (p_Response.m_ResponseStatus != ImapManager::ResponseStatusOk)
  {
    if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed)
    {
      SetDialogMessage("Get folders failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed)
    {
      SetDialogMessage("Get message body failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed)
    {
      SetDialogMessage("Get message headers failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed)
    {
      SetDialogMessage("Get message ids failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed)
    {
      SetDialogMessage("Get message flags failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusLoginFailed)
    {
      const std::string hint = m_ImapManager ? m_ImapManager->GetLastErrorHint()
                                             : std::string();
      SetDialogMessage("Login failed" + (hint.empty() ? "" : ": " + hint), true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusReconnectFailed)
    {
      const std::string hint = m_ImapManager ? m_ImapManager->GetLastErrorHint()
                                             : std::string();
      SetDialogMessage("Reconnect failed" + (hint.empty() ? "" : ": " + hint), true /* p_Warn */);
    }

    uiRequest |= Ui::UiRequestDrawAll;
  }

  AsyncUiRequest(uiRequest);
}

void UiModel::ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    if (!p_Action.m_MoveDestination.empty())
    {
      SetDialogMessage("Move message failed", true /* p_Warn */);
      LOG_WARNING("move destination = %s", p_Action.m_MoveDestination.c_str());
    }
    else if (p_Action.m_SetSeen || p_Action.m_SetUnseen)
    {
      SetDialogMessage("Update message flags failed", true /* p_Warn */);
    }
    else if (p_Action.m_UploadDraft)
    {
      SetDialogMessage("Saving draft failed, message queued for upload", true /* p_Warn */);
      OfflineQueue::PushDraftMessage(p_Action.m_Msg);
    }
    else if (p_Action.m_UploadMessage)
    {
      SetDialogMessage("Importing message failed", true /* p_Warn */);
    }
    else if (p_Action.m_DeleteMessages)
    {
      SetDialogMessage("Permanently delete message failed", true /* p_Warn */);
    }
    else
    {
      SetDialogMessage("Unknown IMAP action error", true /* p_Warn */);
    }
  }
}

void UiModel::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  if (p_Result.m_SmtpStatus != SmtpStatusOk)
  {
    {
      Lock model(this);
      model->m_SmtpErrorResults.push_back(p_Result);
    }

    AsyncUiRequest(Ui::UiRequestDrawError);
  }
  else
  {
    std::string sentFolder;
    std::string draftsFolder;
    std::string trashFolder;
    {
      // folders may be updated by special folder detection in ui thread
      Lock model(this);
      sentFolder = model->m_SentFolder;
      draftsFolder = model->m_DraftsFolder;
      trashFolder = model->m_TrashFolder;
    }

    const SmtpManager::Action& action = p_Result.m_Action;
    const std::vector<Contact> to = Contact::FromStrings(MailUtil::SplitAddrs(action.m_To));
    const std::vector<Contact> cc = Contact::FromStrings(MailUtil::SplitAddrs(action.m_Cc));
    const std::vector<Contact> bcc = Contact::FromStrings(MailUtil::SplitAddrs(action.m_Bcc));

    std::vector<Contact> contacts;
    std::move(to.begin(), to.end(), std::back_inserter(contacts));
    std::move(cc.begin(), cc.end(), std::back_inserter(contacts));
    std::move(bcc.begin(), bcc.end(), std::back_inserter(contacts));

    for (auto& contact : contacts)
    {
      const std::string& address = contact.GetAddress();

      if (address == m_Address)
      {
        InvalidateUiCache(m_Inbox);
        AsyncUiRequest(Ui::UiRequestDrawAll);
        break;
      }
    }

    if ((action.m_ComposeDraftUid != 0) && !draftsFolder.empty() && !trashFolder.empty())
    {
      MoveMessages(std::set<uint32_t>({ action.m_ComposeDraftUid }), draftsFolder,
                   trashFolder);
    }

    if (m_ClientStoreSent)
    {
      if (!sentFolder.empty())
      {
        ImapManager::Action imapAction;
        imapAction.m_UploadMessage = true;
        imapAction.m_Folder = sentFolder;
        imapAction.m_Msg = p_Result.m_Message;
        m_ImapManager->AsyncAction(imapAction);
      }
      else
      {
        SetDialogMessage("Sent folder not configured", true /* p_Warn */);
      }
    }

    if (!sentFolder.empty())
    {
      Lock model(this);
      model->m_HasRequestedUids[sentFolder] = false;
    }

    std::string from = action.m_From;
    if (!from.empty())
    {
      AddressBook::AddFrom(from);
    }
    else
    {
      AddressBook::AddFrom(m_DefaultFrom);
    }
  }

  FileUtil::RmDir(p_Result.m_Action.m_ComposeTempDirectory);
}

void UiModel::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  bool startSync = false;
  {
    Lock model(this);
    model->m_Status.Update(p_StatusUpdate);

    // Update sync state
    if (model->m_SyncState != Ui::SyncStateIdle)
    {
      if (model->m_SyncState == Ui::SyncStateStarted)
      {
        if (model->m_Status.IsSet(Status::FlagPrefetching))
        {
          model->m_SyncState = Ui::SyncStateInProgress;
        }
      }
      else if (model->m_SyncState == Ui::SyncStateInProgress)
      {
        if (!model->m_Status.IsSet(Status::FlagPrefetching))
        {
          LOG_DEBUG("sync completed");
          model->m_SyncState = Ui::SyncStateIdle;
        }
      }
    }

    // Auto-start full sync on connected, if configured for full sync
    if (!model->m_HasRequestedFolders && !model->m_HasPrefetchRequestedFolders &&
        (m_PrefetchLevel >= Ui::PrefetchLevelFullSync) &&
        (p_StatusUpdate.SetFlags & Status::FlagConnected))
    {
      LOG_DEBUG("sync started (auto)");
      model->m_SyncState = Ui::SyncStateStarted;
      model->m_HasPrefetchRequestedFolders = true;
      startSync = true;
    }
  }

  if (startSync)
  {
    ImapManager::Request request;
    request.m_PrefetchLevel = Ui::PrefetchLevelFullSync;
    request.m_GetFolders = true;
    LOG_DEBUG("prefetch req folders");
    m_ImapManager->PrefetchRequest(request);
  }

  char uiRequest = Ui::UiRequestDrawAll;
  if (p_StatusUpdate.SetFlags & Status::FlagConnected)
  {
    uiRequest |= Ui::UiRequestHandleConnected;
  }

  AsyncUiRequest(uiRequest);
}

void UiModel::SearchHandler(const ImapManager::SearchQuery& p_SearchQuery,
                            const ImapManager::SearchResult& p_SearchResult)
{
  {
    Lock model(this);
    if (p_SearchQuery.m_Offset == 0)
    {
      model->m_MessageListSearchResultHeaders = p_SearchResult.m_Headers;
      model->m_MessageListSearchResultFolderUids = p_SearchResult.m_FolderUids;
      LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
    }
    else if (p_SearchQuery.m_Offset > 0)
    {
      model->m_MessageListSearchResultHeaders.insert(model->m_MessageListSearchResultHeaders.end(),
                                                     p_SearchResult.m_Headers.begin(), p_SearchResult.m_Headers.end());
      model->m_MessageListSearchResultFolderUids.insert(model->m_MessageListSearchResultFolderUids.end(),
                                                        p_SearchResult.m_FolderUids.begin(),
                                                        p_SearchResult.m_FolderUids.end());
      LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
    }

    model->m_MessageListSearchHasMore = p_SearchResult.m_HasMore;
    model->m_UpdateUidFromIndex = true;
  }

  AsyncUiRequest(Ui::UiRequestDrawAll);
}

void UiModel::HandleSpecialFolders()
{
  // apply detected special folders for those not configured
  std::map<std::string, std::string> applied;
  {
    Lock model(this);
    const std::map<std::string, std::string*> roleFolders =
    {
      { "sent", &model->m_SentFolder },
      { "drafts", &model->m_DraftsFolder },
      { "trash", &model->m_TrashFolder },
      { "spam", &model->m_SpamFolder },
    };

    for (const auto& detected : model->m_DetectedSpecialFolders)
    {
      auto it = roleFolders.find(detected.first);
      if ((it != roleFolders.end()) && it->second->empty() && !detected.second.empty())
      {
        LOG_INFO("detected %s folder %s", detected.first.c_str(), detected.second.c_str());
        *it->second = detected.second;
        applied[detected.first] = detected.second;
      }
    }

    model->m_DetectedSpecialFolders.clear();
  }

  if (!applied.empty() && m_SpecialFoldersHandler)
  {
    m_SpecialFoldersHandler(applied);
  }
}

void UiModel::SetTrashFolder(const std::string& p_TrashFolder)
{
  Lock model(this);
  model->m_TrashFolder = p_TrashFolder;
}

void UiModel::SetSpamFolder(const std::string& p_SpamFolder)
{
  Lock model(this);
  model->m_SpamFolder = p_SpamFolder;
}

void UiModel::SetDraftsFolder(const std::string& p_DraftsFolder)
{
  Lock model(this);
  model->m_DraftsFolder = p_DraftsFolder;
}

void UiModel::SetSentFolder(const std::string& p_SentFolder)
{
  Lock model(this);
  model->m_SentFolder = p_SentFolder;
}

std::string UiModel::GetTrashFolder()
{
  Lock model(this);
  return model->m_TrashFolder;
}

std::string UiModel::GetSpamFolder()
{
  Lock model(this);
  return model->m_SpamFolder;
}

std::string UiModel::GetDraftsFolder()
{
  Lock model(this);
  return model->m_DraftsFolder;
}

std::string UiModel::GetSentFolder()
{
  Lock model(this);
  return model->m_SentFolder;
}

void UiModel::SetShowProgress(int p_ShowProgress)
{
  Lock model(this);
  model->m_Status.SetShowProgress(p_ShowProgress);
}

void UiModel::SetHasRequestedUids(const std::string& p_Folder, bool p_HasRequestedUids)
{
  Lock model(this);
  model->m_HasRequestedUids[p_Folder] = p_HasRequestedUids;
}

void UiModel::MoveMessages(const std::set<uint32_t>& p_Uids, const std::string& p_From,
                           const std::string& p_To)
{
  ImapManager::Action action;
  action.m_Folder = p_From;
  action.m_Uids = p_Uids;

  static bool copyToTrash = AppUtil::GetCopyToTrash();
  bool isToTrash = false;
  {
    Lock model(this);
    isToTrash = (p_To == model->m_TrashFolder);
  }

  if (copyToTrash && isToTrash)
  {
    action.m_CopyDestination = p_To;
    action.m_DeleteMessages = true;
  }
  else
  {
    action.m_MoveDestination = p_To;
  }

  m_ImapManager->AsyncAction(action);

  const std::string& folder = p_From;

  {
    Lock model(this);

    model->UpdateDisplayUids(folder, action.m_Uids);
    model->m_Uids[folder] = model->m_Uids[folder] - action.m_Uids;
    model->m_Headers[folder] = model->m_Headers[folder] - action.m_Uids;

    model->m_HasRequestedUids[p_From] = false;
    model->m_HasRequestedUids[p_To] = false;
    model->m_UpdateIndexFromUid = true;

    int resultCount = model->m_MessageListSearchResultHeaders.size();
    for (int i = 0; i < resultCount; ++i)
    {
      auto& folderUid = model->m_MessageListSearchResultFolderUids.at(i);
      if ((folderUid.first == folder) && (p_Uids.find(folderUid.second) != p_Uids.end()))
      {
        model->m_MessageListSearchResultFolderUids.erase(model->m_MessageListSearchResultFolderUids.begin() + i);
        model->m_MessageListSearchResultHeaders.erase(model->m_MessageListSearchResultHeaders.begin() + i);
        --resultCount;
      }
    }
  }
}

void UiModel::DeleteMessages(const std::set<uint32_t>& p_Uids, const std::string& p_Folder)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids = p_Uids;
  action.m_DeleteMessages = true;
  m_ImapManager->AsyncAction(action);

  {
    Lock model(this);
    model->UpdateDisplayUids(p_Folder, action.m_Uids);
    model->m_Uids[p_Folder] = model->m_Uids[p_Folder] - action.m_Uids;
    model->m_Headers[p_Folder] = model->m_Headers[p_Folder] - action.m_Uids;

    model->m_HasRequestedUids[p_Folder] = false;
    model->m_UpdateIndexFromUid = true;

    int resultCount = model->m_MessageListSearchResultHeaders.size();
    for (int i = 0; i < resultCount; ++i)
    {
      auto& folderUid = model->m_MessageListSearchResultFolderUids.at(i);
      if ((folderUid.first == p_Folder) && (p_Uids.find(folderUid.second) != p_Uids.end()))
      {
        model->m_MessageListSearchResultFolderUids.erase(model->m_MessageListSearchResultFolderUids.begin() + i);
        model->m_MessageListSearchResultHeaders.erase(model->m_MessageListSearchResultHeaders.begin() + i);
        --resultCount;
      }
    }
  }
}

void UiModel::SetSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Seen)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids = p_Uids;
  action.m_SetSeen = p_Seen;
  action.m_SetUnseen = !p_Seen;
  m_ImapManager->AsyncAction(action);

  {
    Lock model(this);
    for (auto& uid : p_Uids)
    {
      Flag::SetSeen(model->m_Flags[p_Folder][uid], p_Seen);
    }
  }
}

void UiModel::MarkSeen(const std::string& p_Folder, uint32_t p_Uid)
{
  std::map<uint32_t, uint32_t> flags;
  const std::string& folder = p_Folder;
  const int uid = p_Uid;

  {
    Lock model(this);
    flags = model->m_Flags[folder];
  }

  bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));

  if (oldSeen) return;

  bool newSeen = true;

  ImapManager::Action action;
  action.m_Folder = folder;
  action.m_Uids.insert(uid);
  action.m_SetSeen = newSeen;
  action.m_SetUnseen = !newSeen;
  m_ImapManager->AsyncAction(action);

  {
    Lock model(this);
    Flag::SetSeen(model->m_Flags[folder][uid], newSeen);
  }
}

void UiModel::InvalidateUiCache(const std::string& p_Folder)
{
  Lock model(this);
  model->m_HasRequestedUids[p_Folder] = false;
  model->m_Flags[p_Folder].clear();
  model->m_RequestedFlags[p_Folder].clear();
}

void UiModel::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_ImapManager = p_ImapManager;
}

void UiModel::ResetImapManager()
{
  m_ImapManager.reset();
}

std::shared_ptr<ImapManager> UiModel::GetImapManager()
{
  return m_ImapManager;
}

void UiModel::SetSpecialFoldersHandler(const std::function<void(const std::map<std::string, std::string>&)>& p_Handler)
{
  m_SpecialFoldersHandler = p_Handler;
}

void UiModel::SetClientStoreSent(bool p_ClientStoreSent)
{
  m_ClientStoreSent = p_ClientStoreSent;
}

void UiModel::SetNewMsgBell(bool p_NewMsgBell)
{
  m_NewMsgBell = p_NewMsgBell;
}

std::string UiModel::GetInbox()
{
  return m_Inbox;
}

std::string UiModel::GetAddress()
{
  return m_Address;
}

uint32_t UiModel::GetPrefetchLevel()
{
  return m_PrefetchLevel;
}

bool UiModel::GetPrefetchAllHeaders()
{
  return m_PrefetchAllHeaders;
}

void UiModel::Impl::InvalidateUidCache(const std::string& p_Folder)
{
  // called with lock held
  m_Uids.erase(p_Folder);
  m_Headers.erase(p_Folder);
  m_Flags.erase(p_Folder);
  m_Bodys.erase(p_Folder);
  m_HeaderUids.erase(p_Folder);
  m_DisplayUids.erase(p_Folder);
  m_RequestedHeaders.erase(p_Folder);
  m_RequestedFlags.erase(p_Folder);
  m_RequestedBodys.erase(p_Folder);
}

std::map<std::string, uint32_t>& UiModel::Impl::GetDisplayUids(const std::string& p_Folder)
{
  const Ui::SortFilter& sortFilter = m_SortFilter[p_Folder];
  std::map<std::string, uint32_t>& displayUids = m_DisplayUids[p_Folder][sortFilter];
  return displayUids;
}

std::set<uint32_t>& UiModel::Impl::GetHeaderUids(const std::string& p_Folder)
{
  return m_HeaderUids[p_Folder];
}

std::string UiModel::Impl::GetDisplayUidsKey(const std::string& p_Folder, uint32_t p_Uid, Ui::SortFilter p_SortFilter)
{
  std::map<uint32_t, Header>& headers = m_Headers[p_Folder];
  const std::map<uint32_t, uint32_t>& flags = m_Flags[p_Folder];

  std::string key;
  std::string priKey;
  std::map<uint32_t, Header>::iterator hit = headers.find(p_Uid);
  std::string dateUidKey =
    ((hit != headers.end()) ? hit->second.GetDateTime() : "") + " " + StrUtil::ZeroPad(p_Uid, 7);
  std::map<uint32_t, uint32_t>::const_iterator fit;
  switch (p_SortFilter)
  {
    case Ui::SortDefault:
    case Ui::SortDateDesc:
      key = dateUidKey;
      break;

    case Ui::SortDateAsc:
      key = dateUidKey;
      StrUtil::BitInvertString(key);
      break;

    case Ui::SortUnseenOnly:
      fit = flags.find(p_Uid);
      key = ((fit != flags.end()) && !Flag::GetSeen(fit->second)) ? dateUidKey : "";
      break;

    case Ui::SortAttchOnly:
      key = ((hit != headers.end()) && hit->second.GetHasAttachments()) ? dateUidKey : "";
      break;

    case Ui::SortCurrDateOnly:
      key = ((hit != headers.end()) && (hit->second.GetDate() == m_FilterCustomStr)) ? dateUidKey : "";
      break;

    case Ui::SortCurrNameOnly:
      if (hit != headers.end())
      {
        std::string name = (p_Folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
        MailUtil::NormalizeName(name);
        key = (name == m_FilterCustomStr) ? dateUidKey : "";
      }
      else
      {
        key = "";
      }
      break;

    case Ui::SortCurrSubjOnly:
      if (hit != headers.end())
      {
        std::string subj = hit->second.GetSubject();
        MailUtil::NormalizeSubject(subj, true /*p_ToLower*/);
        key = (subj == m_FilterCustomStr) ? dateUidKey : "";
      }
      else
      {
        key = "";
      }
      break;

    case Ui::SortNameDesc:
      if (hit != headers.end())
      {
        priKey = (p_Folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
      }
      else
      {
        priKey = "";
      }
      MailUtil::NormalizeName(priKey);
      key = priKey + " " + dateUidKey;
      break;

    case Ui::SortNameAsc:
      if (hit != headers.end())
      {
        priKey = (p_Folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
      }
      else
      {
        priKey = "";
      }
      MailUtil::NormalizeName(priKey);
      key = priKey + " " + dateUidKey;
      StrUtil::BitInvertString(key);
      break;

    case Ui::SortSubjDesc:
      priKey = ((hit != headers.end()) ? hit->second.GetSubject() : "");
      MailUtil::NormalizeSubject(priKey, true /*p_ToLower*/);
      key = priKey + " " + dateUidKey;
      break;

    case Ui::SortSubjAsc:
      priKey = ((hit != headers.end()) ? hit->second.GetSubject() : "");
      MailUtil::NormalizeSubject(priKey, true /*p_ToLower*/);
      key = priKey + " " + dateUidKey;
      StrUtil::BitInvertString(key);
      break;

    case Ui::SortUnseenDesc:
      fit = flags.find(p_Uid);
      priKey = ((fit != flags.end()) && !Flag::GetSeen(fit->second)) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      break;

    case Ui::SortUnseenAsc:
      fit = flags.find(p_Uid);
      priKey = ((fit != flags.end()) && !Flag::GetSeen(fit->second)) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      StrUtil::BitInvertString(key);
      break;

    case Ui::SortAttchDesc:
      priKey = ((hit != headers.end()) && hit->second.GetHasAttachments()) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      break;

    case Ui::SortAttchAsc:
      priKey = ((hit != headers.end()) && hit->second.GetHasAttachments()) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      StrUtil::BitInvertString(key);
      break;

    default:
      LOG_WARNING("unhandled sortfilter %d", p_SortFilter);
      break;
  }

  return key;
}

// must be called with m_Mutex lock held
void UiModel::Impl::UpdateDisplayUids(const std::string& p_Folder,
                                      const std::set<uint32_t>& p_RemovedUids /*= std::set<uint32_t>()*/,
                                      const std::set<uint32_t>& p_AddedUids /*= std::set<uint32_t>()*/,
                                      bool p_FilterUpdated /*= false*/)
{
  std::set<uint32_t>& headerUids = m_HeaderUids[p_Folder];
  Ui::SortFilter& sortFilter = m_SortFilter[p_Folder];
  std::map<std::string, uint32_t>& displayUids = m_DisplayUids[p_Folder][sortFilter];
  uint64_t& displayUidsVersion = m_DisplayUidsVersion[p_Folder][sortFilter];
  uint64_t& headerUidsVersion = m_HeaderUidsVersion[p_Folder];
  (void)p_FilterUpdated; // @todo: remove unused argument

  if (displayUidsVersion != headerUidsVersion)
  {
    displayUids.clear();
    for (auto& uid : headerUids)
    {
      if (uid == 0) continue;

      std::string key = GetDisplayUidsKey(p_Folder, uid, sortFilter);
      if (key.empty()) continue;

      displayUids.insert(std::pair<std::string, uint32_t>(key, uid));
    }

    displayUidsVersion = headerUidsVersion;
  }

  if (!p_RemovedUids.empty())
  {
    headerUids = headerUids - p_RemovedUids;
    ++headerUidsVersion;

    for (auto& uid : p_RemovedUids)
    {
      if (uid == 0) continue;

      std::string key = GetDisplayUidsKey(p_Folder, uid, sortFilter);
      if (key.empty()) continue;

      auto displayUid = displayUids.find(key);
      if (displayUid != displayUids.end())
      {
        displayUids.erase(displayUid);
      }
    }

    displayUidsVersion = headerUidsVersion;
  }

  if (!p_AddedUids.empty())
  {
    headerUids = headerUids + p_AddedUids;
    ++headerUidsVersion;

    for (auto& uid : p_AddedUids)
    {
      if (uid == 0) continue;

      std::string key = GetDisplayUidsKey(p_Folder, uid, sortFilter);
      if (key.empty()) continue;

      displayUids.insert(std::pair<std::string, uint32_t>(key, uid));
    }

    displayUidsVersion = headerUidsVersion;
  }
}

void UiModel::AsyncUiRequest(char p_UiRequest)
{
  m_UiRequestHandler(p_UiRequest);
}

void UiModel::SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn /*= false */)
{
  m_DialogMessageHandler(p_DialogMessage, p_Warn);
}
