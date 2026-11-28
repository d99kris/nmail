// uimessagelistview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uimessagelistview.h"

#include <algorithm>
#include <climits>
#include <cstdint>

#include <unistd.h>

#include "apputil.h"
#include "fileutil.h"
#include "flag.h"
#include "loghelp.h"
#include "mailutil.h"
#include "sethelp.h"
#include "strutil.h"
#include "uiconfig.h"
#include "uihelpview.h"
#include "uimessageview.h"
#include "uimodel.h"

UiMessageListView::UiMessageListView(Ui* p_Ui)
  : UiScreenView(p_Ui)
{
  m_AttachmentIndicator = UiConfig::GetStr("attachment_indicator");
  m_AutomoveTrashAllow = UiConfig::GetBool("automove_trash_allow");
  m_DeleteWithoutConfirm = UiConfig::GetBool("delete_without_confirm");
  m_PersistSearchQuery = UiConfig::GetBool("persist_search_query");
  m_PersistSelectionOnSortFilterChange = UiConfig::GetBool("persist_selection_on_sortfilter_change");
  m_PersistSortFilter = UiConfig::GetBool("persist_sortfilter");
  m_UnreadIndicator = UiConfig::GetStr("unread_indicator");
  m_SearchShowFolder = UiConfig::GetBool("search_show_folder");
}

void UiMessageListView::Cleanup()
{
  UiConfig::SetBool("search_show_folder", m_SearchShowFolder);
}

void UiMessageListView::Draw()
{
  if (m_MessageListSearch)
  {
    DrawMessageListSearch();
  }
  else
  {
    DrawMessageList();
  }
}

void UiMessageListView::DrawMessageList()
{
  bool requestUids = false;
  {
    UiModel::Lock model(m_Model);
    if (!model->m_HasRequestedUids[m_CurrentFolder])
    {
      model->m_HasRequestedUids[m_CurrentFolder] = true;
      requestUids = true;
    }
  }

  if (requestUids)
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetUids = true;
    LOG_DEBUG_VAR("async req uids =", m_CurrentFolder);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t> fetchFlagUids;
  std::set<uint32_t> fetchBodyPriUids;
  std::set<uint32_t> fetchBodySecUids;
  std::set<uint32_t> prefetchBodyUids;

  {
    UiModel::Lock model(m_Model);
    std::map<uint32_t, Header>& headers = model->m_Headers[m_CurrentFolder];
    std::map<uint32_t, uint32_t>& flags = model->m_Flags[m_CurrentFolder];
    const std::map<std::string, uint32_t>& displayUids = model->GetDisplayUids(m_CurrentFolder);

    std::set<uint32_t>& requestedHeaders = model->m_RequestedHeaders[m_CurrentFolder];
    std::set<uint32_t>& requestedFlags = model->m_RequestedFlags[m_CurrentFolder];
    const std::map<uint32_t, Body>& bodys = model->m_Bodys[m_CurrentFolder];
    std::set<uint32_t>& prefetchedBodys = model->m_PrefetchedBodys[m_CurrentFolder];
    std::set<uint32_t>& requestedBodys = model->m_RequestedBodys[m_CurrentFolder];
    const std::string& currentDate = Header::GetCurrentDate();

    auto selectedUidsIt = m_SelectedUids.find(m_CurrentFolder);
    std::set<uint32_t> noSelection;
    const std::set<uint32_t>& folderSelectedUids =
      (selectedUidsIt != m_SelectedUids.end()) ? selectedUidsIt->second : noSelection;

    if (!m_Model->GetPrefetchAllHeaders())
    {
      // get headers / flags for current view and next page
      int idxOffs = StrUtil::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] -
                                            ((m_MainWinHeight - 1) / 2)),
                                   std::max(0, (int)displayUids.size() - (int)m_MainWinHeight));
      int idxMax = std::min(idxOffs + (m_MainWinHeight * 2), (int)displayUids.size());
      for (int i = idxOffs; i < idxMax; ++i)
      {
        uint32_t uid = std::prev(displayUids.end(), i + 1)->second;

        if ((headers.find(uid) == headers.end()) &&
            (requestedHeaders.find(uid) == requestedHeaders.end()))
        {
          fetchHeaderUids.insert(uid);
          requestedHeaders.insert(uid);
        }

        if ((flags.find(uid) == flags.end()) &&
            (requestedFlags.find(uid) == requestedFlags.end()))
        {
          fetchFlagUids.insert(uid);
          requestedFlags.insert(uid);
        }
      }
    }

    bool hasAttrsSelected = (m_AttrsSelectedItem != A_NORMAL);

    werase(m_MainWin);

    int idxOffs = StrUtil::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] -
                                          ((m_MainWinHeight - 1) / 2)),
                                 std::max(0, (int)displayUids.size() - (int)m_MainWinHeight));
    int idxMax = idxOffs + std::min(m_MainWinHeight, (int)displayUids.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      uint32_t uid = std::prev(displayUids.end(), i + 1)->second;

      bool isUnread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
      static const std::wstring wUnreadIndicator = StrUtil::ToWString(m_UnreadIndicator);
      static const int unreadIndicatorWidth = StrUtil::WStringWidth(wUnreadIndicator);
      std::string unreadFlag = isUnread ? std::string(m_UnreadIndicator)
                                        : std::string(unreadIndicatorWidth, ' ');

      std::string shortDate;
      std::string shortFrom;
      std::string subject;
      std::string attachFlag;
      auto hit = headers.find(uid);
      if (hit != headers.end())
      {
        Header& header = headers.at(uid);
        shortDate = header.GetDateOrTime(currentDate);
        subject = StrUtil::RemoveInvisibleChars(header.GetSubject());
        if (m_CurrentFolder == model->m_SentFolder)
        {
          shortFrom = StrUtil::RemoveInvisibleChars(header.GetShortTo());
        }
        else
        {
          shortFrom = StrUtil::RemoveInvisibleChars(header.GetShortFrom());
        }

        if (!m_AttachmentIndicator.empty())
        {
          static const std::wstring wIndicator = StrUtil::ToWString(m_AttachmentIndicator);
          static const int indicatorWidth = StrUtil::WStringWidth(wIndicator);
          attachFlag = header.GetHasAttachments() ? std::string(m_AttachmentIndicator)
                                                  : std::string(indicatorWidth, ' ');
        }
      }

      bool isSelected = (folderSelectedUids.find(uid) != folderSelectedUids.end());
      std::string selectFlag = (isSelected && !hasAttrsSelected) ? "X" : " ";

      shortDate = StrUtil::TrimPadString(shortDate, 10);
      shortFrom = StrUtil::ToString(StrUtil::TrimPadWString(StrUtil::ToWString(shortFrom), 20));
      std::string headerLeft = selectFlag + unreadFlag + " " + attachFlag + "  " + shortDate + "  " + shortFrom + "  ";
      int subjectWidth = m_ScreenWidth - StrUtil::WStringWidth(StrUtil::ToWString(headerLeft)) - 1;
      subject = StrUtil::ToString(StrUtil::TrimPadWString(StrUtil::ToWString(subject), subjectWidth));
      std::string header = headerLeft + subject + " ";

      bool isCurrent = (i == m_MessageListCurrentIndex[m_CurrentFolder]);

      if (isCurrent)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
      }

      if (isSelected)
      {
        wattron(m_MainWin, isCurrent ? m_AttrsSelectedHighlighted : m_AttrsSelectedItem);
      }

      std::wstring wheader = StrUtil::TrimPadWString(StrUtil::ToWString(header), m_ScreenWidth - 1) + L" ";
      mvwaddnwstr(m_MainWin, i - idxOffs, 0, wheader.c_str(), (int)wheader.size());

      if (isSelected)
      {
        wattroff(m_MainWin, isCurrent ? m_AttrsSelectedHighlighted : m_AttrsSelectedItem);
      }

      if (isCurrent)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentMessage)
          {
            requestedBodys.insert(uid);
            fetchBodyPriUids.insert(uid);
          }
        }
      }
      else if (abs(i - m_MessageListCurrentIndex[m_CurrentFolder]) == 1)
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentView)
          {
            requestedBodys.insert(uid);
            fetchBodySecUids.insert(uid);
          }
        }
      }
      else
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (prefetchedBodys.find(uid) == prefetchedBodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentView)
          {
            prefetchedBodys.insert(uid);
            prefetchBodyUids.insert(uid);
          }
        }
      }
    }
  }

  for (auto& uid : fetchBodyPriUids)
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;

    std::set<uint32_t> fetchUids;
    fetchUids.insert(uid);
    request.m_GetBodys = fetchUids;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req pri bodys =", fetchUids);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  for (auto& uid : fetchBodySecUids)
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;

    std::set<uint32_t> fetchUids;
    fetchUids.insert(uid);
    request.m_GetBodys = fetchUids;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req sec bodys =", fetchUids);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  for (auto& uid : prefetchBodyUids)
  {
    ImapManager::Request request;
    request.m_PrefetchLevel = Ui::PrefetchLevelCurrentView;
    request.m_Folder = m_CurrentFolder;

    std::set<uint32_t> fetchUids;
    fetchUids.insert(uid);
    request.m_GetBodys = fetchUids;

    LOG_DEBUG_VAR("prefetch req bodys =", fetchUids);
    m_Model->GetImapManager()->PrefetchRequest(request);
  }

  const int maxHeadersFetchRequest = 25;
  if (!fetchHeaderUids.empty())
  {
    LOG_DEBUG("fetching %d headers on demand", fetchHeaderUids.size());

    std::set<uint32_t> subsetFetchHeaderUids;
    for (auto it = fetchHeaderUids.begin(); it != fetchHeaderUids.end(); ++it)
    {
      subsetFetchHeaderUids.insert(*it);
      if ((subsetFetchHeaderUids.size() == maxHeadersFetchRequest) ||
          (std::next(it) == fetchHeaderUids.end()))
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetHeaders = subsetFetchHeaderUids;

        LOG_DEBUG_VAR("async req headers =", subsetFetchHeaderUids);
        m_Model->GetImapManager()->AsyncRequest(request);

        subsetFetchHeaderUids.clear();
      }
    }
  }

  const int maxFlagsFetchRequest = 1000;
  if (!fetchFlagUids.empty())
  {
    LOG_DEBUG("fetching %d flags on demand", fetchFlagUids.size());

    std::set<uint32_t> subsetFetchFlagUids;
    for (auto it = fetchFlagUids.begin(); it != fetchFlagUids.end(); ++it)
    {
      subsetFetchFlagUids.insert(*it);
      if ((subsetFetchFlagUids.size() == maxFlagsFetchRequest) ||
          (std::next(it) == fetchFlagUids.end()))
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetFlags = subsetFetchFlagUids;

        LOG_DEBUG_VAR("async req flags =", subsetFetchFlagUids);
        m_Model->GetImapManager()->AsyncRequest(request);

        subsetFetchFlagUids.clear();
      }
    }
  }

  RefreshWin(m_MainWin);
}

void UiMessageListView::DrawMessageListSearch()
{
  std::map<std::string, std::set<uint32_t>> fetchFlagUids;
  std::map<std::string, std::set<uint32_t>> fetchHeaderUids;
  std::map<std::string, std::set<uint32_t>> fetchBodyPriUids;
  std::map<std::string, std::set<uint32_t>> fetchBodySecUids;

  {
    UiModel::Lock model(m_Model);
    std::vector<Header>& headers = model->m_MessageListSearchResultHeaders;
    int idxOffs = StrUtil::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] - ((m_MainWinHeight - 1) / 2)),
                                 std::max(0, (int)headers.size() - (int)m_MainWinHeight));
    int idxMax = idxOffs + std::min(m_MainWinHeight, (int)headers.size());
    const std::string& currentDate = Header::GetCurrentDate();
    bool hasAttrsSelected = (m_AttrsSelectedItem != A_NORMAL);

    werase(m_MainWin);

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& folder = model->m_MessageListSearchResultFolderUids.at(i).first;
      const int uid = model->m_MessageListSearchResultFolderUids.at(i).second;

      bool isUnread = false;
      {
        std::map<uint32_t, uint32_t>& flags = model->m_Flags[folder];
        std::set<uint32_t>& requestedFlags = model->m_RequestedFlags[folder];
        if ((flags.find(uid) == flags.end()) &&
            (requestedFlags.find(uid) == requestedFlags.end()))
        {
          fetchFlagUids[folder].insert(uid);
          requestedFlags.insert(uid);
        }

        isUnread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
      }

      static const std::wstring wUnreadIndicator = StrUtil::ToWString(m_UnreadIndicator);
      static const int unreadIndicatorWidth = StrUtil::WStringWidth(wUnreadIndicator);
      std::string unreadFlag = isUnread ? std::string(m_UnreadIndicator)
                                        : std::string(unreadIndicatorWidth, ' ');

      std::string shortDate;
      std::string shortFrom;
      std::string subject;
      std::string attachFlag;
      {
        Header& header = headers[i];
        shortDate = header.GetDateOrTime(currentDate);
        shortFrom = StrUtil::RemoveInvisibleChars(header.GetShortFrom());
        subject = StrUtil::RemoveInvisibleChars(header.GetSubject());

        if (!m_AttachmentIndicator.empty())
        {
          static const std::wstring wIndicator = StrUtil::ToWString(m_AttachmentIndicator);
          static const int indicatorWidth = StrUtil::WStringWidth(wIndicator);
          attachFlag = header.GetHasAttachments() ? std::string(m_AttachmentIndicator)
                                                  : std::string(indicatorWidth, ' ');
        }
      }

      auto selectedUidsIt = m_SelectedUids.find(folder);
      std::set<uint32_t> noSelection;
      const std::set<uint32_t>& folderSelectedUids =
        (selectedUidsIt != m_SelectedUids.end()) ? selectedUidsIt->second : noSelection;
      bool isSelected = (folderSelectedUids.find(uid) != folderSelectedUids.end());
      std::string selectFlag = (isSelected && !hasAttrsSelected) ? "X" : " ";

      shortDate = StrUtil::TrimPadString(shortDate, 10);
      shortFrom = StrUtil::ToString(StrUtil::TrimPadWString(StrUtil::ToWString(shortFrom), 20));
      std::string headerLeft = selectFlag + unreadFlag + " " + attachFlag + "  " + shortDate + "  " + shortFrom + "  ";

      std::string folderTag = m_SearchShowFolder ? ("  [" + FileUtil::BaseName(folder) + "]") : "";
      int subjectWidth = m_ScreenWidth - StrUtil::WStringWidth(StrUtil::ToWString(headerLeft + folderTag)) - 1;
      subject = StrUtil::ToString(StrUtil::TrimPadWString(StrUtil::ToWString(subject), subjectWidth));
      std::string header = headerLeft + subject + folderTag + " ";

      bool isCurrent = (i == m_MessageListCurrentIndex[m_CurrentFolder]);

      if (isCurrent)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
      }

      if (isSelected)
      {
        wattron(m_MainWin, isCurrent ? m_AttrsSelectedHighlighted : m_AttrsSelectedItem);
      }

      std::wstring wheader = StrUtil::TrimPadWString(StrUtil::ToWString(header), m_ScreenWidth - 1) + L" ";
      mvwaddnwstr(m_MainWin, i - idxOffs, 0, wheader.c_str(), (int)wheader.size());

      if (isSelected)
      {
        wattroff(m_MainWin, isCurrent ? m_AttrsSelectedHighlighted : m_AttrsSelectedItem);
      }

      if (isCurrent)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        const std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
        std::set<uint32_t>& requestedBodys = model->m_RequestedBodys[folder];
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentMessage)
          {
            requestedBodys.insert(uid);
            fetchBodyPriUids[folder].insert(uid);
          }
        }

        const std::map<uint32_t, Header>& gheaders = model->m_Headers[folder];
        std::set<uint32_t>& requestedHeaders = model->m_RequestedHeaders[folder];
        if ((gheaders.find(uid) == gheaders.end()) &&
            (requestedHeaders.find(uid) == requestedHeaders.end()))
        {
          if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentMessage)
          {
            requestedHeaders.insert(uid);
            fetchHeaderUids[folder].insert(uid);
          }
        }
      }
      else if (abs(i - m_MessageListCurrentIndex[m_CurrentFolder]) == 1)
      {
        const std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
        std::set<uint32_t>& requestedBodys = model->m_RequestedBodys[folder];
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentView)
          {
            requestedBodys.insert(uid);
            fetchBodySecUids[folder].insert(uid);
          }
        }
      }
    }
  }

  for (auto& fetchFlagUid : fetchFlagUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchFlagUid.first;
    request.m_GetFlags = fetchFlagUid.second;

    LOG_DEBUG_VAR("async req flags =", request.m_GetFlags);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  for (auto& fetchHeaderUid : fetchHeaderUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchHeaderUid.first;
    request.m_GetHeaders = fetchHeaderUid.second;

    LOG_DEBUG_VAR("async req headers =", request.m_GetHeaders);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  for (auto& fetchBodyUid : fetchBodyPriUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchBodyUid.first;
    request.m_GetBodys = fetchBodyUid.second;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req pri bodys =", request.m_GetBodys);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  for (auto& fetchBodyUid : fetchBodySecUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchBodyUid.first;
    request.m_GetBodys = fetchBodyUid.second;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req sec bodys =", request.m_GetBodys);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  RefreshWin(m_MainWin);
}

void UiMessageListView::KeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if (p_Key == m_KeyRefresh)
  {
    // Refresh is allowed online, and also in shadow read-only sessions
    // catering for getting main instance cache updates.
    if (IsConnected() || AppUtil::GetReadOnly())
    {
      m_Model->InvalidateUiCache(m_CurrentFolder);
    }
    else
    {
      SetDialogMessage("Cannot refresh while offline");
    }
  }
  else if ((p_Key == m_KeyUp) || (p_Key == m_KeyPrevMsg))
  {
    --m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == m_KeyDown) || (p_Key == m_KeyNextMsg))
  {
    ++m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] - m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyNextPage)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyHome)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyEnd)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = std::numeric_limits<int>::max();
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) || (p_Key == m_KeyOpen) || (p_Key == m_KeyRight))
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (m_MessageListSearch)
      {
        m_MessageFindMatchLine = -1;
        m_MessageFindQuery = m_MessageListSearchQuery;

        // remove chars not supported by find in document
        StrUtil::RemoveChar(m_MessageFindQuery, '"');
        StrUtil::RemoveChar(m_MessageFindQuery, '*');
      }

      SetState(Ui::StateViewMessage);
    }
  }
  else if ((p_Key == m_KeyGotoFolder) || (p_Key == m_KeyBack) || (p_Key == m_KeyLeft))
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_HelpView->ResetMessagesListOffset();
      SetCurrentFolder(m_PreviousFolder);
      m_PreviousFolder = "";
      UpdateIndexFromUid();
    }
    else
    {
      if (!m_PersistSortFilter)
      {
        DisableSortFilter();
      }

      SetState(Ui::StateGotoFolder);
    }
  }
  else if (p_Key == m_KeyGotoInbox)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_HelpView->ResetMessagesListOffset();
      m_PreviousFolder = "";
    }

    if (m_CurrentFolder != m_Model->GetInbox())
    {
      SetCurrentFolder(m_Model->GetInbox());
    }
    else
    {
      m_MessageListCurrentIndex[m_CurrentFolder] = 0;
      UpdateUidFromIndex(true /* p_UserTriggered */);
    }

    SetState(Ui::StateViewMessageList);
  }
  else if ((p_Key == m_KeyMove) || (p_Key == m_KeyAutoMove))
  {
    if (IsConnected())
    {
      UpdateUidFromIndex(true /* p_UserTriggered */);
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        m_IsAutoMove = (p_Key != m_KeyMove);
        SetState(Ui::StateMoveToFolder);
      }
      else
      {
        SetDialogMessage("No message to move");
      }
    }
    else
    {
      SetDialogMessage("Cannot move while offline");
    }
  }
  else if (p_Key == m_KeyCompose)
  {
    SetState(Ui::StateComposeMessage);
  }
  else if (p_Key == m_KeyComposeCopy)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (m_MessageView->CurrentMessageBodyHeaderAvailable())
      {
        SetState(Ui::StateComposeCopyMessage);
      }
      else
      {
        SetDialogMessage("Cannot compose copy of message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to copy for compose");
    }
  }
  else if ((p_Key == m_KeyReplyAll) || (p_Key == m_KeyReplySender))
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (m_MessageView->CurrentMessageBodyHeaderAvailable())
      {
        SetState((p_Key == m_KeyReplyAll) ? Ui::StateReplyAllMessage : Ui::StateReplySenderMessage);
      }
      else
      {
        SetDialogMessage("Cannot reply message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to reply");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (m_MessageView->CurrentMessageBodyHeaderAvailable())
      {
        SetState(Ui::StateForwardMessage);
      }
      else
      {
        SetDialogMessage("Cannot forward message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to forward");
    }
  }
  else if (p_Key == m_KeyForwardAttached)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (m_MessageView->CurrentMessageBodyHeaderAvailable())
      {
        SetState(Ui::StateForwardAttachedMessage);
      }
      else
      {
        SetDialogMessage("Cannot forward message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to forward");
    }
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == m_KeyDeleteChar))
  {
    if (IsConnected())
    {
      UpdateUidFromIndex(true /* p_UserTriggered */);
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        DeleteMessage();
      }
      else
      {
        SetDialogMessage("No message to delete");
      }
    }
    else
    {
      SetDialogMessage("Cannot delete while offline");
    }
  }
  else if (p_Key == m_KeyToggleUnread)
  {
    if (IsConnected())
    {
      UpdateUidFromIndex(true /* p_UserTriggered */);
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        ToggleSeen();
      }
      else
      {
        SetDialogMessage("No message to toggle read/unread");
      }
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else if (p_Key == m_KeyOtherCmdHelp)
  {
    m_HelpView->NextMessagesListOffset();
  }
  else if (p_Key == m_KeyExport)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      m_MessageView->ExportMessage();
    }
    else
    {
      SetDialogMessage("No message to export");
    }
  }
  else if (p_Key == m_KeyImport)
  {
    if (IsConnected())
    {
      ImportMessage();
    }
    else
    {
      SetDialogMessage("Cannot import while offline");
    }
  }
  else if (p_Key == m_KeySearch)
  {
    SearchMessage(true /* p_IsLocal */);
  }
  else if (p_Key == m_KeySync)
  {
    StartSync();
  }
  else if (p_Key == m_KeyExtHtmlViewer)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      m_MessageView->ExtHtmlViewer();
    }
    else
    {
      SetDialogMessage("No message to view");
    }
  }
  else if (p_Key == m_KeyExtMsgViewer)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      m_MessageView->ExtMsgViewer();
    }
    else
    {
      SetDialogMessage("No message to view");
    }
  }
  else if ((p_Key == m_KeyFilterSortReset) && !m_MessageListSearch)
  {
    DisableSortFilter();
  }
  else if ((p_Key == m_KeyFilterShowUnread) && !m_MessageListSearch)
  {
    ToggleFilter(Ui::SortUnseenOnly);
  }
  else if ((p_Key == m_KeyFilterShowHasAttachments) && !m_MessageListSearch)
  {
    ToggleFilter(Ui::SortAttchOnly);
  }
  else if ((p_Key == m_KeyFilterShowCurrentDate) && !m_MessageListSearch)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ToggleFilter(Ui::SortCurrDateOnly);
  }
  else if ((p_Key == m_KeyFilterShowCurrentName) && !m_MessageListSearch)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ToggleFilter(Ui::SortCurrNameOnly);
  }
  else if ((p_Key == m_KeyFilterShowCurrentSubject) && !m_MessageListSearch)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ToggleFilter(Ui::SortCurrSubjOnly);
  }
  else if ((p_Key == m_KeySortUnread) && !m_MessageListSearch)
  {
    ToggleSort(Ui::SortUnseenDesc, Ui::SortUnseenAsc);
  }
  else if ((p_Key == m_KeySortHasAttachments) && !m_MessageListSearch)
  {
    ToggleSort(Ui::SortAttchDesc, Ui::SortAttchAsc);
  }
  else if ((p_Key == m_KeySortDate) && !m_MessageListSearch)
  {
    ToggleSort(Ui::SortDateDesc, Ui::SortDateAsc);
  }
  else if ((p_Key == m_KeySortName) && !m_MessageListSearch)
  {
    ToggleSort(Ui::SortNameDesc, Ui::SortNameAsc);
  }
  else if ((p_Key == m_KeySortSubject) && !m_MessageListSearch)
  {
    ToggleSort(Ui::SortSubjDesc, Ui::SortSubjAsc);
  }
  else if ((p_Key == m_KeyJumpTo) && m_MessageListSearch)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_HelpView->ResetMessagesListOffset();
      SetCurrentFolder(m_CurrentFolderUid.first);
      const uint32_t uid = m_CurrentFolderUid.second;

      bool requestUids = false;
      {
        UiModel::Lock model(m_Model);
        if (!model->m_HasRequestedUids[m_CurrentFolder])
        {
          model->m_HasRequestedUids[m_CurrentFolder] = true;
          requestUids = true;
        }
      }

      if (requestUids)
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetUids = true;
        request.m_GetHeaders = std::set<uint32_t>({ uid });
        LOG_DEBUG_VAR("async req uids =", m_CurrentFolder);
        m_Model->GetImapManager()->AsyncRequest(request);

        bool found = false;
        int totalWaitMs = 0;
        const int stepSleepMs = 10;
        const int maxWaitMs = 2000; // max wait for fetching folder headers from cache
        while ((totalWaitMs < maxWaitMs) && !found)
        {
          usleep(stepSleepMs * 1000);
          totalWaitMs += stepSleepMs;
          {
            UiModel::Lock model(m_Model);
            std::map<uint32_t, Header>& headers = model->m_Headers[m_CurrentFolder];
            std::set<uint32_t>& uids = model->m_Uids[m_CurrentFolder];

            if ((headers.find(uid) != headers.end()) && (uids.size() == headers.size()))
            {
              found = true;
            }
          }
        }
      }
      else
      {
        LOG_DEBUG("jump fetch not needed");
      }

      m_MessageListCurrentUid[m_CurrentFolder] = uid;
      m_MessageListUidSet[m_CurrentFolder] = true;
      UpdateIndexFromUid();
    }
  }
  else if (p_Key == m_KeySelectItem)
  {
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      ToggleSelected();

      ++m_MessageListCurrentIndex[m_CurrentFolder];
      UpdateUidFromIndex(true /* p_UserTriggered */);
    }
    else
    {
      SetDialogMessage("No message to select/unselect");
    }
  }
  else if (p_Key == m_KeySelectAll)
  {
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      ToggleSelectAll();
    }
    else
    {
      SetDialogMessage("No messages to select/unselect");
    }
  }
  else if ((p_Key == m_KeySearchShowFolder) && m_MessageListSearch)
  {
    m_SearchShowFolder = !m_SearchShowFolder;
  }
  else if (p_Key == m_KeySearchCurrentSubject)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    SearchMessageBasedOnCurrent(true /* p_Subject */);
  }
  else if (p_Key == m_KeySearchCurrentName)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    SearchMessageBasedOnCurrent(false /* p_Subject */);
  }
  else if (p_Key == m_KeySearchServer)
  {
    if (IsConnected())
    {
      SearchMessage(false /* p_IsLocal */);
    }
    else
    {
      SetDialogMessage("Cannot search server while offline");
    }
  }
  else if (m_InvalidInputNotify)
  {
    SetDialogMessage("Invalid input (" + StrUtil::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void UiMessageListView::UpdateUidFromIndex(bool p_UserTriggered)
{
  if (m_MessageListSearch)
  {
    UiModel::Lock model(m_Model);
    const std::vector<Header>& headers = model->m_MessageListSearchResultHeaders;
    m_MessageListCurrentIndex[m_CurrentFolder] =
      StrUtil::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)headers.size() - 1);

    const int32_t idx = m_MessageListCurrentIndex[m_CurrentFolder];
    if (idx < (int)model->m_MessageListSearchResultFolderUids.size())
    {
      m_CurrentFolderUid.first = model->m_MessageListSearchResultFolderUids[idx].first;
      m_CurrentFolderUid.second = model->m_MessageListSearchResultFolderUids[idx].second;
    }

    if (model->m_MessageListSearchHasMore &&
        ((m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight) >= ((int)headers.size())))
    {
      m_MessageListSearchOffset += m_MessageListSearchMax;
      m_MessageListSearchMax = m_MainWinHeight;
      model->m_MessageListSearchHasMore = false;

      ImapManager::SearchQuery searchQuery;
      searchQuery.m_QueryStr = m_MessageListSearchQuery;
      searchQuery.m_Folder = m_PreviousFolder;
      searchQuery.m_Offset = m_MessageListSearchOffset;
      searchQuery.m_Max = m_MessageListSearchMax;

      LOG_DEBUG("search str = \"%s\" offset = %d max = %d",
                searchQuery.m_QueryStr.c_str(), searchQuery.m_Offset, searchQuery.m_Max);
      m_Model->GetImapManager()->AsyncSearch(m_IsLocalSearch, searchQuery);
    }

    return;
  }

  UiModel::Lock model(m_Model);
  const std::map<std::string, uint32_t>& displayUids = model->GetDisplayUids(m_CurrentFolder);

  m_MessageListCurrentIndex[m_CurrentFolder] =
    StrUtil::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)displayUids.size() - 1);
  if (displayUids.size() > 0)
  {
    m_MessageListCurrentUid[m_CurrentFolder] =
      std::prev(displayUids.end(), m_MessageListCurrentIndex[m_CurrentFolder] + 1)->second;
  }
  else
  {
    m_MessageListCurrentUid[m_CurrentFolder] = -1;
  }

  m_CurrentFolderUid.first = m_CurrentFolder;
  m_CurrentFolderUid.second = m_MessageListCurrentUid[m_CurrentFolder];

  m_MessageListUidSet[m_CurrentFolder] = p_UserTriggered;

  static int lastUid = 0;
  if (lastUid != m_MessageListCurrentUid[m_CurrentFolder])
  {
    m_MessageViewToggledSeen = false;
    lastUid = m_MessageListCurrentUid[m_CurrentFolder];
  }

  LOG_TRACE("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder],
            m_MessageListCurrentIndex[m_CurrentFolder]);
}

void UiMessageListView::UpdateIndexFromUid()
{
  if (m_MessageListSearch) return;

  bool found = false;

  {
    UiModel::Lock model(m_Model);

    if (m_MessageListUidSet[m_CurrentFolder])
    {
      const std::map<std::string, uint32_t>& displayUids = model->GetDisplayUids(m_CurrentFolder);

      for (auto it = displayUids.rbegin(); it != displayUids.rend(); ++it)
      {
        if ((int32_t)it->second == m_MessageListCurrentUid[m_CurrentFolder])
        {
          m_MessageListCurrentIndex[m_CurrentFolder] = std::distance(displayUids.rbegin(), it);
          found = true;
          break;
        }
      }
    }
  }

  if (!found)
  {
    UpdateUidFromIndex(false /* p_UserTriggered */);
  }
  else
  {
    m_CurrentFolderUid.first = m_CurrentFolder;
    m_CurrentFolderUid.second = m_MessageListCurrentUid[m_CurrentFolder];
  }

  LOG_TRACE("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder],
            m_MessageListCurrentIndex[m_CurrentFolder]);
}

void UiMessageListView::SyncCursor()
{
  bool updateIndexFromUid = false;
  bool updateUidFromIndex = false;
  {
    UiModel::Lock model(m_Model);
    std::swap(updateIndexFromUid, model->m_UpdateIndexFromUid);
    std::swap(updateUidFromIndex, model->m_UpdateUidFromIndex);
  }

  if (updateIndexFromUid)
  {
    UpdateIndexFromUid();
  }

  if (updateUidFromIndex)
  {
    UpdateUidFromIndex(false /* p_UserTriggered */);
  }
}

void UiMessageListView::SearchMessageBasedOnCurrent(bool p_Subject)
{
  std::string current;
  bool found = false;

  if (m_MessageListSearch)
  {
    UiModel::Lock model(m_Model);
    std::vector<Header>& headers = model->m_MessageListSearchResultHeaders;
    int idx = m_MessageListCurrentIndex[m_CurrentFolder];
    if ((idx >= 0) && (idx < (int)headers.size()))
    {
      current = p_Subject ? headers[idx].GetSubject()
                          : headers[idx].GetShortFrom();
      found = true;
    }
  }
  else
  {
    UiModel::Lock model(m_Model);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit != headers.end())
    {
      current = p_Subject ? hit->second.GetSubject()
                          : ((folder != model->m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo());
      found = true;
    }
  }

  if (found)
  {
    current = StrUtil::Trim(current);
    if (p_Subject)
    {
      MailUtil::NormalizeSubject(current, true /*p_ToLower*/);
    }
    else
    {
      MailUtil::NormalizeName(current);
    }
    std::string query = p_Subject ? ("subject:\"" + current + "\"")
                                  : ("from:\"" + current + "\"");
    SearchMessage(true /* p_IsLocal */, query);
  }
  else
  {
    SetDialogMessage("No message selected to search based on");
  }
}

void UiMessageListView::SearchMessage(bool p_IsLocal, const std::string& p_Query /*= std::string()*/)
{
  std::string query = !p_Query.empty()
    ? p_Query
    : ((m_MessageListSearch && m_PersistSearchQuery) ? m_MessageListSearchQuery : "");
  if (!p_Query.empty() || PromptString("Search Emails: ", "Search", query))
  {
    m_IsLocalSearch = p_IsLocal;
    if (!query.empty())
    {
      m_MessageListSearch = true;
      m_HelpView->ResetMessagesListOffset();
      if (m_CurrentFolder != "")
      {
        m_PreviousFolder = m_CurrentFolder;
        SetCurrentFolder("");
      }

      m_MessageListCurrentIndex[m_CurrentFolder] = 0;
      ClearSelection();

      {
        UiModel::Lock model(m_Model);
        m_MessageListSearchQuery = query;
        m_MessageListSearchOffset = 0;
        m_MessageListSearchMax = m_MainWinHeight + m_MainWinHeight;
        model->m_MessageListSearchHasMore = false;
        model->m_MessageListSearchResultHeaders.clear();
        model->m_MessageListSearchResultFolderUids.clear();
      }

      ImapManager::SearchQuery searchQuery;
      searchQuery.m_QueryStr = query;
      searchQuery.m_Folder = m_PreviousFolder;
      searchQuery.m_Offset = 0;
      searchQuery.m_Max = 2 * m_MainWinHeight;

      LOG_DEBUG("search str=\"%s\" offset=%d max=%d",
                searchQuery.m_QueryStr.c_str(), searchQuery.m_Offset, searchQuery.m_Max);
      m_Model->GetImapManager()->AsyncSearch(m_IsLocalSearch, searchQuery);
    }
    else
    {
      m_MessageListSearch = false;
      m_HelpView->ResetMessagesListOffset();
      ClearSelection();
      if (m_PreviousFolder != "")
      {
        SetCurrentFolder(m_PreviousFolder);
        m_PreviousFolder = "";
      }

      UpdateIndexFromUid();
    }
  }
}

void UiMessageListView::SortFilterPreUpdate()
{
  if (m_PersistSelectionOnSortFilterChange)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
}

void UiMessageListView::SortFilterUpdated(bool p_FilterUpdated)
{
  {
    UiModel::Lock model(m_Model);
    model->UpdateDisplayUids(m_CurrentFolder, std::set<uint32_t>(), std::set<uint32_t>(), p_FilterUpdated);
  }

  if (m_PersistSelectionOnSortFilterChange)
  {
    UpdateIndexFromUid();
  }
  else
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
}

void UiMessageListView::DisableSortFilter()
{
  SortFilterPreUpdate();
  bool wasFilterEnabled = false;
  {
    UiModel::Lock model(m_Model);
    Ui::SortFilter& sortFilter = model->m_SortFilter[m_CurrentFolder];
    wasFilterEnabled = (sortFilter == Ui::SortUnseenOnly) || (sortFilter == Ui::SortAttchOnly);
    sortFilter = Ui::SortDefault;
  }

  SortFilterUpdated(wasFilterEnabled);
}

void UiMessageListView::ToggleFilter(Ui::SortFilter p_SortFilter)
{
  SortFilterPreUpdate();
  if (!SetFilter(p_SortFilter))
  {
    SetDialogMessage("No message selected to filter on");
    return;
  }

  SortFilterUpdated(true /* p_FilterUpdated */);
}

bool UiMessageListView::SetFilter(Ui::SortFilter p_SortFilter)
{
  UiModel::Lock model(m_Model);
  Ui::SortFilter& sortFilter = model->m_SortFilter[m_CurrentFolder];
  Ui::SortFilter newSortFilter = (sortFilter != p_SortFilter) ? p_SortFilter : Ui::SortDefault;

  if ((newSortFilter == Ui::SortCurrDateOnly) || (newSortFilter == Ui::SortCurrNameOnly) ||
      (newSortFilter == Ui::SortCurrSubjOnly))
  {
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Header>& headers = model->m_Headers[m_CurrentFolder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit == headers.end())
    {
      return false;
    }

    // dont cache custom date, name or subject filters due to mem usage
    std::map<std::string, uint32_t>& displayUids = model->m_DisplayUids[m_CurrentFolder][newSortFilter];
    uint64_t& displayUidsVersion = model->m_DisplayUidsVersion[m_CurrentFolder][newSortFilter];
    displayUids.clear();
    displayUidsVersion = 0;

    switch (newSortFilter)
    {
      case Ui::SortCurrDateOnly:
        model->m_FilterCustomStr = hit->second.GetDate();
        break;

      case Ui::SortCurrNameOnly:
        model->m_FilterCustomStr = (m_CurrentFolder != model->m_SentFolder)
          ? hit->second.GetShortFrom()
          : hit->second.GetShortTo();
        MailUtil::NormalizeName(model->m_FilterCustomStr);
        break;

      case Ui::SortCurrSubjOnly:
        model->m_FilterCustomStr = hit->second.GetSubject();
        MailUtil::NormalizeSubject(model->m_FilterCustomStr, true /*p_ToLower*/);
        break;

      default:
        break;
    }
  }

  sortFilter = newSortFilter;
  return true;
}

void UiMessageListView::ToggleSort(Ui::SortFilter p_SortFirst, Ui::SortFilter p_SortSecond)
{
  SortFilterPreUpdate();
  bool wasFilterEnabled = false;
  {
    UiModel::Lock model(m_Model);
    Ui::SortFilter& sortFilter = model->m_SortFilter[m_CurrentFolder];
    wasFilterEnabled = (sortFilter == Ui::SortUnseenOnly) || (sortFilter == Ui::SortAttchOnly);
    sortFilter = (sortFilter == p_SortSecond)
      ? Ui::SortDefault
      : ((sortFilter == p_SortFirst) ? p_SortSecond : p_SortFirst);
  }

  SortFilterUpdated(wasFilterEnabled);
}

void UiMessageListView::ClearSelection()
{
  m_SelectedUids.clear();
  m_AllSelected = false;
}

void UiMessageListView::ToggleSelected()
{
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  std::set<uint32_t>& folderSelectedUids = m_SelectedUids[folder];
  auto it = folderSelectedUids.find(uid);
  if (it == folderSelectedUids.end())
  {
    folderSelectedUids.insert(uid);
    SetDialogMessage("Selected message");
  }
  else
  {
    folderSelectedUids.erase(uid);
    if (folderSelectedUids.empty())
    {
      m_SelectedUids.erase(folder);
    }

    SetDialogMessage("Unselected message");
  }
}

void UiMessageListView::ToggleSelectAll()
{
  m_SelectedUids.clear();
  if (m_AllSelected)
  {
    m_AllSelected = false;
    SetDialogMessage("Unselected all");
    return;
  }

  int selectCount = 0;
  if (m_MessageListSearch)
  {
    UiModel::Lock model(m_Model);
    std::vector<Header>& headers = model->m_MessageListSearchResultHeaders;
    int idxMax = headers.size();
    for (int i = 0; i < idxMax; ++i)
    {
      const std::string& folder = model->m_MessageListSearchResultFolderUids.at(i).first;
      const int uid = model->m_MessageListSearchResultFolderUids.at(i).second;
      m_SelectedUids[folder].insert(uid);
      ++selectCount;
    }
  }
  else
  {
    std::set<uint32_t>& folderSelectedUids = m_SelectedUids[m_CurrentFolder];

    UiModel::Lock model(m_Model);
    const std::map<std::string, uint32_t>& displayUids = model->GetDisplayUids(m_CurrentFolder);
    for (auto& displayUid : displayUids)
    {
      folderSelectedUids.insert(displayUid.second);
      ++selectCount;
    }
  }

  SetDialogMessage("Selected all " + std::to_string(selectCount) + " messages");
  m_AllSelected = true;
}

int UiMessageListView::GetSelectedCount()
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    selectCount += selectedFolder.second.size();
  }

  return selectCount;
}

std::string UiMessageListView::AutoMoveSelectFolder()
{
  LOG_DEBUG_FUNC(STR());

  std::string subject;
  std::string sender;
  std::string folder;
  std::string selfSender = m_Name;
  std::string foundFolder;
  std::string sentFolder;
  std::string trashFolder;
  static const int maxSearchCount = 10;
  static const int minLengthPrefix = 8;

  {
    UiModel::Lock model(m_Model);
    uint32_t uid = 0;
    auto selectedUidsIt = m_SelectedUids.find(m_CurrentFolder);
    if ((selectedUidsIt != m_SelectedUids.end()) && (!selectedUidsIt->second.empty()))
    {
      folder = m_CurrentFolder;
      uid = *(selectedUidsIt->second.begin());
    }
    else
    {
      folder = m_CurrentFolderUid.first;
      uid = m_CurrentFolderUid.second;
    }

    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit != headers.end())
    {
      subject = StrUtil::Trim(hit->second.GetSubject());
      sender = StrUtil::Trim(((folder != model->m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo()));
    }

    sentFolder = model->m_SentFolder;
    trashFolder = model->m_TrashFolder;
  }

  MailUtil::NormalizeSubject(subject, true /*p_ToLower*/);
  MailUtil::NormalizeName(sender);
  MailUtil::NormalizeName(selfSender);
  const std::string subjectPrefix = subject.substr(0, subject.find(" ", minLengthPrefix));

  const std::string queryNotCurrent = (!folder.empty() ? " AND NOT folder:\"" + folder + "\"" : "");
  const std::string queryNotSent = (!sentFolder.empty()) ? " AND NOT folder:\"" + sentFolder + "\"" : "";
  const std::string queryNotTrash = (!trashFolder.empty()) ? " AND NOT folder:\"" + trashFolder + "\"" : "";

  const std::string queryCommon = queryNotCurrent + queryNotSent + queryNotTrash;
  const std::string queryCommonAllowTrash = queryNotCurrent + queryNotSent;

  std::vector<std::string> queryStrs;
  std::vector<std::string> queryStrsAllowTrash;
  if (!subject.empty())
  {
    // full subject
    const std::string querySpecific = "subject:\"" + subject + "\"";
    queryStrs.push_back(querySpecific + queryCommon);
    queryStrsAllowTrash.push_back(querySpecific + queryCommonAllowTrash);
  }

  if (!subjectPrefix.empty() && !sender.empty())
  {
    // subject prefix and sender name
    const std::string querySpecific = "subject:\"" + subjectPrefix + "\"* AND from:\"" + sender + "\"";
    queryStrs.push_back(querySpecific + queryCommon);
    queryStrsAllowTrash.push_back(querySpecific + queryCommonAllowTrash);
  }

  if (!subjectPrefix.empty())
  {
    // subject prefix
    const std::string querySpecific = "subject:\"" + subjectPrefix + "\"*";
    queryStrs.push_back(querySpecific + queryCommon);
    queryStrsAllowTrash.push_back(querySpecific + queryCommonAllowTrash);
  }

  if (m_AutomoveTrashAllow)
  {
    queryStrs.insert(queryStrs.end(), queryStrsAllowTrash.begin(), queryStrsAllowTrash.end());
  }

  if (!sender.empty())
  {
    // sender name
    const std::string querySpecific = "from:\"" + sender + "\"";
    queryStrs.push_back(querySpecific + queryCommon);
    if (m_AutomoveTrashAllow)
    {
      queryStrs.push_back(querySpecific + queryCommonAllowTrash);
    }
  }

  if (!queryStrs.empty())
  {
    for (const auto& queryStr : queryStrs)
    {
      ImapManager::SearchQuery searchQuery(queryStr, "", 0, maxSearchCount);
      ImapManager::SearchResult searchResult;
      m_Model->GetImapManager()->SyncSearch(true /*p_IsLocal*/, searchQuery, searchResult);
      if (searchResult.m_FolderUids.empty())
      {
        LOG_DEBUG("no matches for query %s", queryStr.c_str());
      }
      else
      {
        foundFolder = searchResult.m_FolderUids.begin()->first;
        LOG_DEBUG("found %s for query %s", foundFolder.c_str(), queryStr.c_str());
        break;
      }
    }
  }
  else
  {
    LOG_DEBUG("skip search subject \"%s\" sender \"%s\"", subject.c_str(), sender.c_str());
  }

  return !foundFolder.empty() ? foundFolder : folder;
}

bool UiMessageListView::DeleteMessage()
{
  const std::string trashFolder = m_Model->GetTrashFolder();
  if (!trashFolder.empty())
  {
    std::set<std::string> foldersAllowPermanentDelete = { trashFolder, m_Model->GetSpamFolder() };
    foldersAllowPermanentDelete.erase("");
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
      int count = UiMessageListView::GetSelectedCount();
      std::string prompt = (count > 1)
        ? "Permanently delete " + std::to_string(count) + " messages (y/n)?"
        : "Permanently delete message (y/n)?";
      if (PromptYesNo(prompt))
      {
        DeleteSelectedMessages();
        ClearSelection();
      }
    }
    else
    {
      int count = UiMessageListView::GetSelectedCount();
      std::string prompt = (count > 1) ? "Delete " + std::to_string(count) + " messages (y/n)?"
                                       : "Delete message (y/n)?";

      if (m_DeleteWithoutConfirm || PromptYesNo(prompt))
      {
        MoveSelectedMessages(trashFolder, m_CurrentFolderUid);

        if (count > 0)
        {
          ClearSelection();
        }

        m_MessageFindMatchLine = -1;
        m_MessageViewLineOffset = 0;

        bool isHeaderUidsEmpty = false;
        {
          UiModel::Lock model(m_Model);
          isHeaderUidsEmpty = model->GetHeaderUids(folder).empty();
        }

        if (isHeaderUidsEmpty)
        {
          SetState(Ui::StateViewMessageList);
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

void UiMessageListView::MoveSelectedMessages(const std::string& p_To,
                                             const std::pair<std::string, int32_t>& p_FolderUid)
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    if (!selectedFolder.second.empty())
    {
      m_Model->MoveMessages(selectedFolder.second, selectedFolder.first, p_To);
    }
    selectCount += selectedFolder.second.size();
  }

  if (selectCount == 0)
  {
    const std::string& folder = p_FolderUid.first;
    const uint32_t uid = p_FolderUid.second;
    m_Model->MoveMessages(std::set<uint32_t>({ uid }), folder, p_To);
  }

  SyncCursor();
}

void UiMessageListView::DeleteSelectedMessages()
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    if (!selectedFolder.second.empty())
    {
      m_Model->DeleteMessages(selectedFolder.second, selectedFolder.first);
    }
    selectCount += selectedFolder.second.size();
  }

  if (selectCount == 0)
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const uint32_t uid = m_CurrentFolderUid.second;
    m_Model->DeleteMessages(std::set<uint32_t>({ uid }), folder);
  }

  SyncCursor();
}

void UiMessageListView::ToggleSeen()
{
  if (m_SelectedUids.empty())
  {
    std::map<std::string, std::set<uint32_t>> folderUids;
    folderUids[m_CurrentFolderUid.first].insert(m_CurrentFolderUid.second);
    m_Model->ToggleSeen(folderUids);
  }
  else
  {
    m_Model->ToggleSeen(m_SelectedUids);
  }
}

void UiMessageListView::ImportMessage()
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
        m_Model->GetImapManager()->AsyncAction(imapAction);
        m_Model->SetHasRequestedUids(m_CurrentFolder, false);
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

void UiMessageListView::OnEnter()
{
  curs_set(0);
  m_HelpView->ResetMessagesListOffset();
  {
    UiModel::Lock model(m_Model);
    m_MessageViewToggledSeen = false;
  }
}
