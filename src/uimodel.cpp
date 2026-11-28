// uimodel.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

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

void Ui::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  if (!s_Running) return;

  char uiRequest = UiRequestNone;

  bool updateIndexFromUid = false;

  if (p_Request.m_PrefetchLevel < PrefetchLevelFullSync)
  {
    std::set<uint32_t> fetchHeaderUids;
    std::set<uint32_t> fetchFlagUids;

    if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Folders = p_Response.m_Folders;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new folders =", p_Response.m_Folders);

      if (!p_Response.m_SpecialFolders.empty())
      {
        m_DetectedSpecialFolders = p_Response.m_SpecialFolders;
        uiRequest |= UiRequestSpecialFolders;
      }
    }

    if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);

      if (p_Response.m_UidInvalid)
      {
        LOG_INFO("folder %s uid invalid", p_Response.m_Folder.c_str());
        InvalidateUidCache(p_Response.m_Folder);
      }

      const std::set<uint32_t> newUids = p_Response.m_Uids - m_Uids[p_Response.m_Folder];
      if (!p_Response.m_Cached && (p_Response.m_Folder == m_Inbox) && !newUids.empty())
      {
        if (m_NewMsgBell)
        {
          LOG_DEBUG("bell");
          beep();
        }
      }

      const std::set<uint32_t>& removedUids = m_Uids[p_Response.m_Folder] - p_Response.m_Uids;
      if (!removedUids.empty())
      {
        LOG_DEBUG_VAR("del uids =", removedUids);
        UpdateDisplayUids(p_Response.m_Folder, removedUids);
        m_Headers[p_Response.m_Folder] = m_Headers[p_Response.m_Folder] - removedUids;
      }

      m_Uids[p_Response.m_Folder] = p_Response.m_Uids;
      uiRequest |= UiRequestDrawAll;
      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new uids =", newUids);

      if (!m_PrefetchAllHeaders && !newUids.empty())
      {
        UpdateDisplayUids(p_Response.m_Folder, std::set<uint32_t>(), newUids);
      }

      if (m_PrefetchAllHeaders)
      {
        std::map<uint32_t, Header>& headers = m_Headers[p_Response.m_Folder];
        std::map<uint32_t, uint32_t>& flags = m_Flags[p_Response.m_Folder];
        std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[p_Response.m_Folder];
        std::set<uint32_t>& requestedFlags = m_RequestedFlags[p_Response.m_Folder];
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
      std::lock_guard<std::mutex> lock(m_Mutex);

      const std::map<uint32_t, Header>& headers = p_Response.m_Headers;

      m_Headers[p_Response.m_Folder].insert(headers.begin(), headers.end());
      if (m_PrefetchAllHeaders)
      {
        UpdateDisplayUids(p_Response.m_Folder, std::set<uint32_t>(), MapKey(headers));
      }
      uiRequest |= UiRequestDrawAll;
      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new headers =", MapKey(headers));
    }

    if (!p_Request.m_GetFlags.empty() &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      std::map<uint32_t, uint32_t> newFlags = p_Response.m_Flags;
      newFlags.insert(m_Flags[p_Response.m_Folder].begin(), m_Flags[p_Response.m_Folder].end());
      m_Flags[p_Response.m_Folder] = newFlags;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new flags =", MapKey(p_Response.m_Flags));
    }

    if (!p_Request.m_GetBodys.empty() &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Bodys[p_Response.m_Folder].insert(p_Response.m_Bodys.begin(), p_Response.m_Bodys.end());
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new bodys =", MapKey(p_Response.m_Bodys));
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

  if (p_Request.m_PrefetchLevel == PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      for (auto& folder : p_Response.m_Folders)
      {
        if (!s_Running)
        {
          break;
        }

        // Allow re-fetching folder uids, to enable full sync more than once per session
        {
          ImapManager::Request request;
          request.m_PrefetchLevel = PrefetchLevelFullSync;
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
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (p_Response.m_UidInvalid)
        {
          LOG_INFO("folder %s uid invalid", p_Response.m_Folder.c_str());
          InvalidateUidCache(p_Response.m_Folder);
        }

        std::map<uint32_t, Header>& headers = m_Headers[folder];
        std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[folder];
        std::set<uint32_t>& prefetchedHeaders = m_PrefetchedHeaders[folder];

        std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
        std::set<uint32_t>& requestedFlags = m_RequestedFlags[folder];
        std::set<uint32_t>& prefetchedFlags = m_PrefetchedFlags[folder];

        std::map<uint32_t, Body>& bodys = m_Bodys[folder];
        std::set<uint32_t>& requestedBodys = m_RequestedBodys[folder];
        std::set<uint32_t>& prefetchedBodys = m_PrefetchedBodys[folder];

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
          if (!s_Running) break;

          subsetPrefetchHeaders.insert(*it);
          if ((subsetPrefetchHeaders.size() == maxHeadersFetchRequest) ||
              (std::next(it) == prefetchHeaders.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
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
          if (!s_Running) break;

          subsetPrefetchFlags.insert(*it);
          if ((subsetPrefetchFlags.size() == maxFlagsFetchRequest) ||
              (std::next(it) == prefetchFlags.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
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
          if (!s_Running) break;

          subsetPrefetchBodys.insert(*it);
          if ((subsetPrefetchBodys.size() == maxBodysFetchRequest) ||
              (std::next(it) == prefetchBodys.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
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

    uiRequest |= UiRequestDrawAll;
  }

  if (updateIndexFromUid)
  {
    UpdateIndexFromUid();
  }

  AsyncUiRequest(uiRequest);
}

void Ui::ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result)
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

void Ui::SmtpResultHandlerError(const SmtpManager::Result& p_Result)
{
  bool saveDraft = false;
  SmtpManager::Action smtpAction = p_Result.m_Action;
  std::string draftMessage;

  if (!m_DraftsFolder.empty())
  {
    if (smtpAction.m_IsSendCreatedMessage)
    {
      SetDialogMessage("Failed sending queued message, uploading draft");
      saveDraft = true;
      draftMessage = smtpAction.m_CreatedMsg;
    }
    else
    {
      const std::string errStr = Smtp::GetErrorMessage(p_Result.m_SmtpStatus);
      const std::string errMsg = (!errStr.empty())
        ? "Send failed (" + errStr + ")."
        : "Send failed.";
      const std::string msg =
        (smtpAction.m_ComposeDraftUid != 0)
        ? errMsg + " Overwrite draft (y) or queue send (n)?"
        : errMsg + " Save draft (y) or queue send (n)?";
      if (PromptYesNo(msg))
      {
        saveDraft = true;
        smtpAction.m_IsSendMessage = false;
        smtpAction.m_IsCreateMessage = true;

        SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
        if (smtpResult.m_SmtpStatus == SmtpStatusOk)
        {
          draftMessage = smtpResult.m_Message;
        }
        else
        {
          SetDialogMessage("Message creation failed", true /* p_Warn */);
          return;
        }
      }
    }
  }

  if (saveDraft)
  {
    ImapManager::Action imapAction;
    imapAction.m_UploadDraft = true;
    imapAction.m_Folder = m_DraftsFolder;
    imapAction.m_Msg = draftMessage;
    m_ImapManager->AsyncAction(imapAction);

    if (smtpAction.m_ComposeDraftUid != 0)
    {
      MoveMessages(std::set<uint32_t>({ smtpAction.m_ComposeDraftUid }), m_DraftsFolder,
                   m_TrashFolder);
      m_HasRequestedUids[m_TrashFolder] = false;
    }

    m_HasRequestedUids[m_DraftsFolder] = false;
  }
  else
  {
    if (!smtpAction.m_IsSendCreatedMessage)
    {
      smtpAction.m_IsSendMessage = false;
      smtpAction.m_IsCreateMessage = true;
      SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
      if (smtpResult.m_SmtpStatus == SmtpStatusOk)
      {
        draftMessage = smtpResult.m_Message;
      }
      else
      {
        SetDialogMessage("Message creation failed", true /* p_Warn */);
      }
    }

    if (!draftMessage.empty())
    {
      OfflineQueue::PushOutboxMessage(draftMessage);
      SetDialogMessage("Message queued for sending");
    }
  }

  AsyncUiRequest(UiRequestDrawAll);
}

void Ui::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  if (p_Result.m_SmtpStatus != SmtpStatusOk)
  {
    std::unique_lock<std::mutex> lock(m_SmtpErrorMutex);
    m_SmtpErrorResults.push_back(p_Result);
    AsyncUiRequest(UiRequestDrawError);
  }
  else
  {
    std::string sentFolder;
    std::string draftsFolder;
    std::string trashFolder;
    {
      // folders may be updated by special folder detection in ui thread
      std::lock_guard<std::mutex> lock(m_Mutex);
      sentFolder = m_SentFolder;
      draftsFolder = m_DraftsFolder;
      trashFolder = m_TrashFolder;
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
        AsyncUiRequest(UiRequestDrawAll);
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
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_HasRequestedUids[sentFolder] = false;
    }

    std::string from = action.m_From;
    if (!from.empty())
    {
      AddressBook::AddFrom(from);
    }
    else
    {
      AddressBook::AddFrom(GetDefaultFrom());
    }
  }

  FileUtil::RmDir(p_Result.m_Action.m_ComposeTempDirectory);
}

void Ui::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Status.Update(p_StatusUpdate);

  // Update sync state
  if (m_SyncState != SyncStateIdle)
  {
    if (m_SyncState == SyncStateStarted)
    {
      if (m_Status.IsSet(Status::FlagPrefetching))
      {
        m_SyncState = SyncStateInProgress;
      }
    }
    else if (m_SyncState == SyncStateInProgress)
    {
      if (!m_Status.IsSet(Status::FlagPrefetching))
      {
        LOG_DEBUG("sync completed");
        m_SyncState = SyncStateIdle;
      }
    }
  }

  // Auto-start full sync on connected, if configured for full sync
  if (!m_HasRequestedFolders && !m_HasPrefetchRequestedFolders && (m_PrefetchLevel >= PrefetchLevelFullSync) &&
      (p_StatusUpdate.SetFlags & Status::FlagConnected))
  {
    LOG_DEBUG("sync started (auto)");
    m_SyncState = SyncStateStarted;

    ImapManager::Request request;
    request.m_PrefetchLevel = PrefetchLevelFullSync;
    request.m_GetFolders = true;
    LOG_DEBUG("prefetch req folders");
    m_HasPrefetchRequestedFolders = true;
    m_ImapManager->PrefetchRequest(request);
  }

  char uiRequest = UiRequestDrawAll;
  if (p_StatusUpdate.SetFlags & Status::FlagConnected)
  {
    uiRequest |= UiRequestHandleConnected;
  }

  AsyncUiRequest(uiRequest);
}

void Ui::SearchHandler(const ImapManager::SearchQuery& p_SearchQuery,
                       const ImapManager::SearchResult& p_SearchResult)
{
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    if (p_SearchQuery.m_Offset == 0)
    {
      m_MessageListSearchResultHeaders = p_SearchResult.m_Headers;
      m_MessageListSearchResultFolderUids = p_SearchResult.m_FolderUids;
      LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
    }
    else if (p_SearchQuery.m_Offset > 0)
    {
      m_MessageListSearchResultHeaders.insert(m_MessageListSearchResultHeaders.end(),
                                              p_SearchResult.m_Headers.begin(), p_SearchResult.m_Headers.end());
      m_MessageListSearchResultFolderUids.insert(m_MessageListSearchResultFolderUids.end(),
                                                 p_SearchResult.m_FolderUids.begin(),
                                                 p_SearchResult.m_FolderUids.end());
      LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
    }

    m_MessageListSearchHasMore = p_SearchResult.m_HasMore;
  }

  AsyncUiRequest(UiRequestDrawAll);
  UpdateUidFromIndex(false /* p_UserTriggered */);
}

void Ui::HandleSpecialFolders()
{
  // apply detected special folders for those not configured
  std::map<std::string, std::string> applied;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::map<std::string, std::string*> roleFolders =
    {
      { "sent", &m_SentFolder },
      { "drafts", &m_DraftsFolder },
      { "trash", &m_TrashFolder },
      { "spam", &m_SpamFolder },
    };

    for (const auto& detected : m_DetectedSpecialFolders)
    {
      auto it = roleFolders.find(detected.first);
      if ((it != roleFolders.end()) && it->second->empty() && !detected.second.empty())
      {
        LOG_INFO("detected %s folder %s", detected.first.c_str(), detected.second.c_str());
        *it->second = detected.second;
        applied[detected.first] = detected.second;
      }
    }

    m_DetectedSpecialFolders.clear();
  }

  if (!applied.empty() && m_SpecialFoldersHandler)
  {
    m_SpecialFoldersHandler(applied);
  }
}

bool Ui::DeleteMessage()
{
  if (!m_TrashFolder.empty())
  {
    static const std::set<std::string> foldersAllowPermanentDelete = [&]()
    {
      std::set<std::string> folders = { m_TrashFolder, m_SpamFolder };
      folders.erase("");
      return folders;
    }();
    const std::string& folder = m_CurrentFolderUid.first;
    const bool hasSelection = !m_SelectedUids.empty();
    const bool permanentDelete = hasSelection
      ? std::all_of(m_SelectedUids.begin(), m_SelectedUids.end(),
                    [&](const std::pair<std::string, std::set<uint32_t>>& sel)
    {
      return sel.second.empty() ||
             (foldersAllowPermanentDelete.count(sel.first) > 0);
    })
      : (foldersAllowPermanentDelete.count(folder) > 0);
    if (permanentDelete)
    {
      int count = Ui::GetSelectedCount();
      std::string prompt = (count > 1)
        ? "Permanently delete " + std::to_string(count) + " messages (y/n)?"
        : "Permanently delete message (y/n)?";
      if (Ui::PromptYesNo(prompt))
      {
        DeleteSelectedMessages();
        ClearSelection();
      }
    }
    else
    {
      int count = Ui::GetSelectedCount();
      std::string prompt = (count > 1) ? "Delete " + std::to_string(count) + " messages (y/n)?"
                                       : "Delete message (y/n)?";

      if (m_DeleteWithoutConfirm || Ui::PromptYesNo(prompt))
      {
        MoveSelectedMessages(m_TrashFolder);

        if (count > 0)
        {
          ClearSelection();
        }

        m_MessageFindMatchLine = -1;
        m_MessageViewLineOffset = 0;

        bool isHeaderUidsEmpty = false;
        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          isHeaderUidsEmpty = GetHeaderUids(folder).empty();
        }

        if (isHeaderUidsEmpty)
        {
          SetState(StateViewMessageList);
        }
      }
    }

    return true;
  }
  else
  {
    SetDialogMessage("Trash folder not configured", true /* p_Warn */);
    return false;
  }
}

void Ui::MoveSelectedMessages(const std::string& p_To)
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    if (!selectedFolder.second.empty())
    {
      MoveMessages(selectedFolder.second, selectedFolder.first, p_To);
    }
    selectCount += selectedFolder.second.size();
  }

  if (selectCount == 0)
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const uint32_t uid = m_CurrentFolderUid.second;
    MoveMessages(std::set<uint32_t>({ uid }), folder, p_To);
  }
}

void Ui::MoveMessages(const std::set<uint32_t>& p_Uids, const std::string& p_From,
                      const std::string& p_To)
{
  ImapManager::Action action;
  action.m_Folder = p_From;
  action.m_Uids = p_Uids;

  static bool copyToTrash = AppUtil::GetCopyToTrash();
  bool isToTrash = false;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    isToTrash = (p_To == m_TrashFolder);
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
    std::lock_guard<std::mutex> lock(m_Mutex);

    UpdateDisplayUids(folder, action.m_Uids);
    m_Uids[folder] = m_Uids[folder] - action.m_Uids;
    m_Headers[folder] = m_Headers[folder] - action.m_Uids;

    m_HasRequestedUids[p_From] = false;
    m_HasRequestedUids[p_To] = false;
  }

  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    int resultCount = m_MessageListSearchResultHeaders.size();
    for (int i = 0; i < resultCount; ++i)
    {
      auto& folderUid = m_MessageListSearchResultFolderUids.at(i);
      if ((folderUid.first == folder) && (p_Uids.find(folderUid.second) != p_Uids.end()))
      {
        m_MessageListSearchResultFolderUids.erase(m_MessageListSearchResultFolderUids.begin() + i);
        m_MessageListSearchResultHeaders.erase(m_MessageListSearchResultHeaders.begin() + i);
        --resultCount;
      }
    }
  }

  UpdateIndexFromUid();
}

void Ui::DeleteSelectedMessages()
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    if (!selectedFolder.second.empty())
    {
      DeleteMessages(selectedFolder.second, selectedFolder.first);
    }
    selectCount += selectedFolder.second.size();
  }

  if (selectCount == 0)
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const uint32_t uid = m_CurrentFolderUid.second;
    DeleteMessages(std::set<uint32_t>({ uid }), folder);
  }
}

void Ui::DeleteMessages(const std::set<uint32_t>& p_Uids, const std::string& p_Folder)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids = p_Uids;
  action.m_DeleteMessages = true;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    UpdateDisplayUids(p_Folder, action.m_Uids);
    m_Uids[p_Folder] = m_Uids[p_Folder] - action.m_Uids;
    m_Headers[p_Folder] = m_Headers[p_Folder] - action.m_Uids;

    m_HasRequestedUids[p_Folder] = false;
  }

  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    int resultCount = m_MessageListSearchResultHeaders.size();
    for (int i = 0; i < resultCount; ++i)
    {
      auto& folderUid = m_MessageListSearchResultFolderUids.at(i);
      if ((folderUid.first == p_Folder) && (p_Uids.find(folderUid.second) != p_Uids.end()))
      {
        m_MessageListSearchResultFolderUids.erase(m_MessageListSearchResultFolderUids.begin() + i);
        m_MessageListSearchResultHeaders.erase(m_MessageListSearchResultHeaders.begin() + i);
        --resultCount;
      }
    }
  }

  UpdateIndexFromUid();
}

void Ui::ToggleSeen()
{
  if (m_SelectedUids.empty())
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, uint32_t> flags;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      flags = m_Flags[folder];
    }
    bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));
    bool newSeen = !oldSeen;
    std::set<uint32_t> uids;
    uids.insert(uid);

    SetSeen(folder, uids, newSeen);
  }
  else
  {
    bool newSeen = true;
    bool newSeenSet = false;
    for (auto& selectedFolder : m_SelectedUids)
    {
      if (!selectedFolder.second.empty())
      {
        if (!newSeenSet)
        {
          std::map<uint32_t, uint32_t> flags;
          {
            std::lock_guard<std::mutex> lock(m_Mutex);
            flags = m_Flags[selectedFolder.first];
          }

          uint32_t firstUid = *selectedFolder.second.rbegin();
          bool oldSeen = ((flags.find(firstUid) != flags.end()) && (Flag::GetSeen(flags.at(firstUid))));
          newSeen = !oldSeen;
          newSeenSet = true;
        }

        SetSeen(selectedFolder.first, selectedFolder.second, newSeen);
      }
    }
  }
}

void Ui::SetSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Seen)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids = p_Uids;
  action.m_SetSeen = p_Seen;
  action.m_SetUnseen = !p_Seen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& uid : p_Uids)
    {
      Flag::SetSeen(m_Flags[p_Folder][uid], p_Seen);
    }
  }
}

void Ui::MarkSeen()
{
  std::map<uint32_t, uint32_t> flags;
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    flags = m_Flags[folder];
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
    std::lock_guard<std::mutex> lock(m_Mutex);
    Flag::SetSeen(m_Flags[folder][uid], newSeen);
  }
}

void Ui::InvalidateUiCache(const std::string& p_Folder)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_HasRequestedUids[p_Folder] = false;
  m_Flags[p_Folder].clear();
  m_RequestedFlags[p_Folder].clear();
}

void Ui::InvalidateUidCache(const std::string& p_Folder)
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

void Ui::ImportMessage()
{
  std::string filename = FileUtil::GetDownloadsDir();
  if (PromptString("Import Filename: ", "Load", filename))
  {
    if (!filename.empty())
    {
      filename = FileUtil::ExpandPath(filename);
      if (FileUtil::NotEmpty(filename))
      {
        const std::string& msg = FileUtil::ReadFile(filename);

        ImapManager::Action imapAction;
        imapAction.m_UploadMessage = true;
        imapAction.m_Folder = m_CurrentFolder;
        imapAction.m_Msg = msg;
        m_ImapManager->AsyncAction(imapAction);
        m_HasRequestedUids[m_CurrentFolder] = false;
      }
      else
      {
        SetDialogMessage("Import failed (file not found or empty)");
      }
    }
    else
    {
      SetDialogMessage("Import cancelled (empty filename)");
    }
  }
  else
  {
    SetDialogMessage("Import cancelled");
  }
}
