// uimessageview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uimessageview.h"

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <sstream>

#include <unistd.h>

#include "extutil.h"
#include "fileutil.h"
#include "flag.h"
#include "loghelp.h"
#include "strutil.h"
#include "textstyle.h"
#include "textutil.h"
#include "uiconfig.h"
#include "uiexternal.h"
#include "uihelpview.h"
#include "uimessagelistview.h"

UiMessageView::UiMessageView(Ui* p_Ui)
  : UiScreenView(p_Ui)
{
  m_AlignHeaderLabels = UiConfig::GetBool("align_header_labels");
  m_FullHeaderIncludeLocal = UiConfig::GetBool("full_header_include_local");
  m_PersistFindQuery = UiConfig::GetBool("persist_find_query");
  m_PlainViewLinks = UiConfig::GetBool("plain_view_links");
  m_RespectFormatFlowed = UiConfig::GetBool("respect_format_flowed");
  m_RewrapQuotedLines = UiConfig::GetBool("rewrap_quoted_lines");
  m_TabSize = StrUtil::Bound(1, UiConfig::GetNum("tab_size"), 80);
  m_MarkdownView = UiConfig::GetBool("markdown_view");
  m_MarkdownViewLinks = m_MarkdownView && UiConfig::GetBool("markdown_view_links");
  m_ShowCompactHeader = UiConfig::GetBool("show_compact_header");
}

void UiMessageView::Cleanup()
{
  UiConfig::SetBool("show_compact_header", m_ShowCompactHeader);
}

void UiMessageView::Draw()
{
  werase(m_MainWin);

  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t> fetchBodyPriUids;
  std::set<uint32_t> fetchBodySecUids;
  bool markSeen = false;
  bool unseen = false;
  {
    UiModel::Lock model(m_Model);

    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::set<uint32_t>& requestedHeaders = model->m_RequestedHeaders[folder];

    if ((uid != -1) &&
        (headers.find(uid) == headers.end()) &&
        (requestedHeaders.find(uid) == requestedHeaders.end()))
    {
      requestedHeaders.insert(uid);
      fetchHeaderUids.insert(uid);
    }

    std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
    std::set<uint32_t>& requestedBodys = model->m_RequestedBodys[folder];

    if ((uid != -1) &&
        (bodys.find(uid) == bodys.end()) &&
        (requestedBodys.find(uid) == requestedBodys.end()))
    {
      requestedBodys.insert(uid);
      fetchBodyPriUids.insert(uid);
    }

    std::map<uint32_t, uint32_t>& flags = model->m_Flags[folder];
    if ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))))
    {
      unseen = true;
    }

    std::string headerText;
    std::map<uint32_t, Header>::iterator headerIt = headers.find(uid);
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);

    std::stringstream ss;
    if (headerIt != headers.end())
    {
      Header& header = headerIt->second;
      if (m_ShowFullHeader)
      {
        ss << header.GetRawHeaderText(m_FullHeaderIncludeLocal);
      }
      else if (m_ShowCompactHeader)
      {
        // names only, without reply-to
        ss << GetHeaderLabel("Date") << header.GetDateTime() << "\n";
        ss << GetHeaderLabel("From") << header.GetShortFrom() << "\n";
        ss << GetHeaderLabel("To") << header.GetShortTo() << "\n";
        if (!header.GetCc().empty())
        {
          ss << GetHeaderLabel("Cc") << header.GetShortCc() << "\n";
        }

        if (!header.GetBcc().empty())
        {
          ss << GetHeaderLabel("Bcc") << header.GetShortBcc() << "\n";
        }

        ss << GetHeaderLabel("Subject") << header.GetSubject() << "\n";
      }
      else
      {
        ss << GetHeaderLabel("Date") << header.GetDateTime() << "\n";
        ss << GetHeaderLabel("From") << header.GetFrom() << "\n";
        if (!header.GetReplyTo().empty())
        {
          ss << GetHeaderLabel("Reply-To") << header.GetReplyTo() << "\n";
        }

        ss << GetHeaderLabel("To") << header.GetTo() << "\n";
        if (!header.GetCc().empty())
        {
          ss << GetHeaderLabel("Cc") << header.GetCc() << "\n";
        }

        if (!header.GetBcc().empty())
        {
          ss << GetHeaderLabel("Bcc") << header.GetBcc() << "\n";
        }

        ss << GetHeaderLabel("Subject") << header.GetSubject() << "\n";
      }

      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        std::map<ssize_t, PartInfo> parts = body.GetPartInfos();
        std::vector<std::string> attnames;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
          if (!it->second.m_Filename.empty())
          {
            attnames.push_back(it->second.m_Filename);
          }
        }

        if (!attnames.empty())
        {
          ss << GetHeaderLabel(m_AlignHeaderLabels ? "Attchmnt" : "Attachments");
          ss << StrUtil::Join(attnames, ", ");
          ss << "\n";
        }
      }

      ss << "\n";
    }

    headerText = ss.str();

    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& bodyText = GetBodyText(body);
      const std::string text = headerText + bodyText;
      m_CurrentMessageViewText = text;
      m_CurrentMessageProcessFlowed = m_RespectFormatFlowed && m_Plaintext && body.IsFormatFlowed();
      m_CurrentMessageFromHtml = m_Plaintext ? !body.HasTextPlain() : body.HasTextHtml(); // each falls back to the other
      std::vector<std::wstring> wlines = GetCachedWordWrapLines(folder, bodyIt->first);
      int countLines = wlines.size();

      m_MessageViewLineOffset = StrUtil::Bound(0, m_MessageViewLineOffset,
                                               countLines - m_MainWinHeight);
      for (int i = 0; ((i < m_MainWinHeight) && (i < countLines)); ++i)
      {
        const int lineIndex = i + m_MessageViewLineOffset;
        const std::wstring& wdispStr = wlines.at(lineIndex);
        const bool isQuote = (wdispStr.rfind(L">", 0) == 0) && (lineIndex > m_MessageViewHeaderLineCount);

        // per-char attrs from base, styled spans and search match
        std::vector<int> attrs(wdispStr.size(), isQuote ? m_AttrsQuotedText : A_NORMAL);
        if (lineIndex < (int)m_MessageViewSpans.size())
        {
          for (const auto& span : m_MessageViewSpans.at(lineIndex))
          {
            const bool isCurrentLink = (span.m_Link >= 0) && (span.m_Link == m_MessageViewLinkIndex);
            const int spanAttrs =
              isCurrentLink ? TextStyle::CombineAttrs(span.m_Attrs, m_AttrsHighlightedLink) : span.m_Attrs;
            for (int k = span.m_Start; (k < (span.m_Start + span.m_Len)) && (k < (int)attrs.size()); ++k)
            {
              attrs[k] = TextStyle::CombineAttrs(attrs[k], spanAttrs);
            }
          }
        }

        if (!m_MessageFindQuery.empty() && (m_MessageFindMatchLine == lineIndex))
        {
          const size_t matchLen = m_MessageFindMatchLen;
          for (size_t k = m_MessageFindMatchPos; (k < (m_MessageFindMatchPos + matchLen)) && (k < attrs.size()); ++k)
          {
            attrs[k] = TextStyle::CombineAttrs(attrs[k], m_AttrsHighlightedText);
          }
        }

        wmove(m_MainWin, i, 0);
        size_t runStart = 0;
        while (runStart < wdispStr.size())
        {
          size_t runEnd = runStart + 1;
          while ((runEnd < wdispStr.size()) && (attrs[runEnd] == attrs[runStart]))
          {
            ++runEnd;
          }

          wattrset(m_MainWin, attrs[runStart]);
          waddnwstr(m_MainWin, wdispStr.c_str() + runStart, runEnd - runStart);
          runStart = runEnd;
        }

        wattrset(m_MainWin, A_NORMAL);
      }

      markSeen = true;
    }

    if (m_Model->GetPrefetchLevel() >= Ui::PrefetchLevelCurrentView)
    {
      const std::map<std::string, uint32_t>& displayUids = model->GetDisplayUids(m_CurrentFolder);
      if (displayUids.size() > 0)
      {
        int32_t maxIndex = (int)model->GetDisplayUids(m_CurrentFolder).size() - 1;
        int32_t nextIndex = StrUtil::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder] + 1, maxIndex);
        int32_t prevIndex = StrUtil::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder] - 1, maxIndex);
        uint32_t nextUid = std::prev(displayUids.end(), nextIndex + 1)->second;
        uint32_t prevUid = std::prev(displayUids.end(), prevIndex + 1)->second;

        if ((bodys.find(nextUid) == bodys.end()) &&
            (requestedBodys.find(nextUid) == requestedBodys.end()))
        {
          requestedBodys.insert(nextUid);
          fetchBodySecUids.insert(nextUid);
        }

        if ((bodys.find(prevUid) == bodys.end()) &&
            (requestedBodys.find(prevUid) == requestedBodys.end()))
        {
          requestedBodys.insert(prevUid);
          fetchBodySecUids.insert(prevUid);
        }
      }
    }
  }

  if (!fetchHeaderUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetHeaders = fetchHeaderUids;
    LOG_DEBUG_VAR("async req headers =", fetchHeaderUids);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  if (!fetchBodyPriUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetBodys = fetchBodyPriUids;
    request.m_ProcessHtml = !m_Plaintext;
    LOG_DEBUG_VAR("async req pri bodys =", fetchBodyPriUids);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  if (!fetchBodySecUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetBodys = fetchBodySecUids;
    request.m_ProcessHtml = !m_Plaintext;
    LOG_DEBUG_VAR("async req sec bodys =", fetchBodySecUids);
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  if (unseen && markSeen && !m_MessageViewToggledSeen)
  {
    m_Model->MarkSeen(folder, uid);
  }

  RefreshWin(m_MainWin);
}

void UiMessageView::KeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if (p_Key == m_KeyPrevMsg)
  {
    int prevIndex = m_MessageListCurrentIndex[m_CurrentFolder]--;
    m_MessageListView->UpdateUidFromIndex(true /* p_UserTriggered */);
    if (prevIndex == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      SetDialogMessage("Already on first message");
    }
    else
    {
      m_MessageViewLineOffset = 0;
      m_MessageFindMatchLine = -1;
      m_MessageViewLinkIndex = -1;
      m_MessageViewLinkPastEnd = false;
    }
  }
  else if (p_Key == m_KeyNextMsg)
  {
    int prevIndex = m_MessageListCurrentIndex[m_CurrentFolder]++;
    m_MessageListView->UpdateUidFromIndex(true /* p_UserTriggered */);
    if (prevIndex == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      SetDialogMessage("No more messages");
    }
    else
    {
      m_MessageViewLineOffset = 0;
      m_MessageFindMatchLine = -1;
      m_MessageViewLinkIndex = -1;
      m_MessageViewLinkPastEnd = false;
    }
  }
  else if ((m_MarkdownViewLinks || m_PlainViewLinks) && ((p_Key == m_KeyNextLink) || (p_Key == m_KeyPrevLink)))
  {
    SelectLink(p_Key == m_KeyNextLink);
  }
  else if ((p_Key == m_KeyOpenLink) && IsLinkSelected())
  {
    OpenLink();
  }
  else if (HandleListKey(p_Key, m_MessageViewLineOffset))
  {
    // none
  }
  else if (p_Key == m_KeySpace) // alternative next page key
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset + m_MainWinHeight;
  }
  else if ((p_Key == m_KeyBackspace) || (p_Key == m_KeyBackspaceAlt) || (p_Key == m_KeyBack) || (p_Key == m_KeyLeft))
  {
    SetState(Ui::StateViewMessageList);
  }
  else if ((p_Key == m_KeyOpen) || (p_Key == m_KeyRight))
  {
    SetState(Ui::StateViewPartList);
  }
  else if (p_Key == m_KeyGotoFolder)
  {
    SetState(Ui::StateGotoFolder);
  }
  else if (p_Key == m_KeyGotoInbox)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_PreviousFolder = "";
    }

    SetCurrentFolder(m_Model->GetInbox());
    SetState(Ui::StateViewMessageList);
  }
  else if ((p_Key == m_KeyMove) || (p_Key == m_KeyAutoMove))
  {
    if (IsConnected())
    {
      m_MessageListView->ClearSelection();
      m_IsAutoMove = (p_Key != m_KeyMove);
      SetState(Ui::StateMoveToFolder);
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
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState(Ui::StateComposeCopyMessage);
    }
    else
    {
      SetDialogMessage("Cannot compose copy of message not fetched");
    }
  }
  else if ((p_Key == m_KeyReplyAll) || (p_Key == m_KeyReplySender))
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState((p_Key == m_KeyReplyAll) ? Ui::StateReplyAllMessage : Ui::StateReplySenderMessage);
    }
    else
    {
      SetDialogMessage("Cannot reply message not fetched");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState(Ui::StateForwardMessage);
    }
    else
    {
      SetDialogMessage("Cannot forward message not fetched");
    }
  }
  else if (p_Key == m_KeyForwardAttached)
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState(Ui::StateForwardAttachedMessage);
    }
    else
    {
      SetDialogMessage("Cannot forward message not fetched");
    }
  }
  else if (p_Key == m_KeyToggleTextHtml)
  {
    m_Plaintext = !m_Plaintext;
    m_MessageViewLineOffset = 0;
    m_MessageFindMatchLine = -1;
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == m_KeyDeleteChar))
  {
    if (IsConnected())
    {
      m_MessageListView->ClearSelection();
      m_MessageListView->DeleteMessage();
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
      {
        UiModel::Lock model(m_Model);
        m_MessageViewToggledSeen = true;
      }
      m_MessageListView->ToggleSeen();
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else if (p_Key == m_KeyOtherCmdHelp)
  {
    m_HelpView->NextMessageOffset();
  }
  else if (p_Key == m_KeyExport)
  {
    ExportMessage();
  }
  else if ((p_Key == m_KeySaveFile) || (p_Key == m_KeySaveMarkdown))
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SaveMessageView(p_Key == m_KeySaveMarkdown);
    }
    else
    {
      SetDialogMessage("Cannot save message not fetched");
    }
  }
  else if (p_Key == m_KeyExtPager)
  {
    m_External->ExtPager(m_CurrentMessageViewText);
  }
  else if (p_Key == m_KeyExtHtmlViewer)
  {
    ExtHtmlViewer();
  }
  else if (p_Key == m_KeyExtMsgViewer)
  {
    ExtMsgViewer();
  }
  else if (p_Key == m_KeyFind)
  {
    MessageFind();
  }
  else if (p_Key == m_KeyFindNext)
  {
    if (!m_MessageFindQuery.empty())
    {
      MessageFindNext();
    }
    else
    {
      SetDialogMessage("Find text not set");
    }
  }
  else if (p_Key == m_KeyToggleFullHeader)
  {
    m_ShowFullHeader = !m_ShowFullHeader;
    m_MessageViewLineOffset = 0;
    m_MessageFindMatchLine = -1;
  }
  else if (p_Key == m_KeyToggleCompactHeader)
  {
    m_ShowCompactHeader = !m_ShowCompactHeader;
    m_MessageViewLineOffset = 0;
    m_MessageFindMatchLine = -1;
  }
  else if (m_InvalidInputNotify)
  {
    SetDialogMessage("Invalid input (" + StrUtil::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

bool UiMessageView::CurrentMessageBodyHeaderAvailable()
{
  UiModel::Lock model(m_Model);
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  const std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
  std::map<uint32_t, Header>& headers = model->m_Headers[folder];
  std::map<uint32_t, Body>::const_iterator bit = bodys.find(uid);
  std::map<uint32_t, Header>::iterator hit = headers.find(uid);
  return ((hit != headers.end()) && (bit != bodys.end()));
}

// selected link in current message and view, links are from last message laid out
bool UiMessageView::IsLinkSelected()
{
  if ((m_MessageViewLinksFolderUid != m_CurrentFolderUid) || (m_MessageViewLinkIndex < 0) ||
      (m_MessageViewLinkIndex >= (int)m_MessageViewLinks.size())) return false;

  const int line = m_MessageViewLinks.at(m_MessageViewLinkIndex).m_Line;
  return (line >= m_MessageViewLineOffset) && (line < (m_MessageViewLineOffset + m_MainWinHeight));
}

void UiMessageView::SelectLink(bool p_Next)
{
  if (m_MessageViewLinks.empty() || (m_MessageViewLinksFolderUid != m_CurrentFolderUid))
  {
    SetDialogMessage("No links in message");
    return;
  }

  // step from current link if visible, or past first / last (if not scrolled since, when restart
  // from view after scroll is enabled), otherwise from view
  static const bool pastEndRestartOnScroll = false;
  const bool pastEnd = m_MessageViewLinkPastEnd &&
    (!pastEndRestartOnScroll || (m_MessageViewLinkPastEndOffset == m_MessageViewLineOffset));
  const int count = m_MessageViewLinks.size();
  const int viewBegin = m_MessageViewLineOffset;
  const int viewEnd = m_MessageViewLineOffset + m_MainWinHeight;
  auto getLine = [&](int p_Index)
  {
    return (int)m_MessageViewLinks.at(p_Index).m_Line;
  };

  int index = -1;
  if ((m_MessageViewLinkIndex >= 0) && (m_MessageViewLinkIndex < count) &&
      (getLine(m_MessageViewLinkIndex) >= viewBegin) && (getLine(m_MessageViewLinkIndex) < viewEnd))
  {
    index = m_MessageViewLinkIndex + (p_Next ? 1 : -1);
  }
  else if (pastEnd)
  {
    index = StrUtil::Bound(-1, m_MessageViewLinkIndex, count) + (p_Next ? 1 : -1);
  }
  else if (p_Next)
  {
    index = 0;
    while ((index < count) && (getLine(index) < viewBegin))
    {
      ++index;
    }
  }
  else
  {
    index = count - 1;
    while ((index >= 0) && (getLine(index) >= viewEnd))
    {
      --index;
    }
  }

  // none selected beyond first and last, as in message find
  if ((index < 0) || (index >= count))
  {
    m_MessageViewLinkIndex = p_Next ? count : -1;
    m_MessageViewLinkPastEnd = true;
    m_MessageViewLinkPastEndOffset = m_MessageViewLineOffset;
    SetDialogMessage("No more links");
    return;
  }

  m_MessageViewLinkIndex = index;
  m_MessageViewLinkPastEnd = false;
  const int line = getLine(index);
  if (line < viewBegin)
  {
    m_MessageViewLineOffset = line;
  }
  else if (line >= viewEnd)
  {
    m_MessageViewLineOffset = line - m_MainWinHeight + 1;
  }

  SetDialogMessage(StrUtil::ToString(m_MessageViewLinks.at(index).m_Url));
}

void UiMessageView::OpenLink()
{
  if (!IsLinkSelected()) return;

  const std::string url = StrUtil::ToString(m_MessageViewLinks.at(m_MessageViewLinkIndex).m_Url);
  if (url.empty()) return;

  if (StrUtil::ToLower(url.substr(0, 7)) == "mailto:")
  {
    m_ComposeMailto = url;
    SetState(Ui::StateComposeMessage);
    return;
  }

  m_External->ExtLinkOpen(url);
}

void UiMessageView::ExportMessage()
{
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::string filename = FileUtil::GetDownloadsDir() + std::to_string(uid) + ".eml";
  if (PromptString("Export Filename: ", "Save", filename))
  {
    if (!filename.empty())
    {
      filename = FileUtil::ExpandPath(filename);
      bool exported = false;
      {
        UiModel::Lock model(m_Model);
        const std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
        if (bodys.find(uid) != bodys.end())
        {
          FileUtil::WriteFile(filename, model->m_Bodys[folder][uid].GetData());
          exported = true;
        }
      }

      if (exported)
      {
        SetDialogMessage("Message exported");
      }
      else
      {
        SetDialogMessage("Export failed (message not available)", true /* p_Warn */);
      }
    }
    else
    {
      SetDialogMessage("Export cancelled (empty filename)");
    }
  }
  else
  {
    SetDialogMessage("Export cancelled");
  }
}

void UiMessageView::ExtHtmlViewer()
{
  static const std::string& tempPath = FileUtil::GetTempDir() + std::string("htmlview/tmp.html");
  FileUtil::DeleteFile(tempPath);

  const std::string folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  {
    UiModel::Lock model(m_Model);
    std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& html = body.GetHtml(); // falls back to text/plain if no html
      FileUtil::WriteFile(tempPath, html);
    }
  }

  if (FileUtil::Exists(tempPath))
  {
    int rv = m_External->ExtHtmlViewer(tempPath);
    if (rv == 0)
    {
      m_Model->MarkSeen(folder, uid);
    }
  }
  else
  {
    SetDialogMessage("View html failed (message not available)", true /* p_Warn */);
  }
}

void UiMessageView::ExtMsgViewer()
{
  static const std::string& tempPath = FileUtil::GetTempDir() + std::string("msgview/tmp.eml");
  FileUtil::DeleteFile(tempPath);

  const std::string folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  {
    UiModel::Lock model(m_Model);
    std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& data = body.GetData(); // falls back to text/plain if no html
      FileUtil::WriteFile(tempPath, data);
    }
  }

  if (FileUtil::Exists(tempPath))
  {
    int rv = m_External->ExtMsgViewer(tempPath);
    if (rv == 0)
    {
      m_Model->MarkSeen(folder, uid);
    }
  }
  else
  {
    SetDialogMessage("View message failed (message not available)", true /* p_Warn */);
  }
}

void UiMessageView::SaveMessageView(bool p_Markdown)
{
  // markdown only from built-in html converter
  const bool hasMarkdown = m_CurrentMessageFromHtml && ExtUtil::GetHtmlToTextConvertCmd().empty();
  if (p_Markdown && !hasMarkdown)
  {
    SetDialogMessage("Markdown not available");
    return;
  }

  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::string subject;
  {
    UiModel::Lock model(m_Model);
    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit != headers.end())
    {
      subject = hit->second.GetSubject();
    }
  }

  // default filename <uid>-subject, with subject non-alphanumerics replaced and max 40 chars
  static const size_t maxSubjectLen = 40;
  std::wstring wsubject;
  for (wchar_t ch : StrUtil::ToWString(subject))
  {
    if (wsubject.size() >= maxSubjectLen) break;

    if (iswalnum(ch))
    {
      wsubject += ch;
    }
    else if (!wsubject.empty() && (wsubject.back() != L'-'))
    {
      wsubject += L'-';
    }
  }

  while (!wsubject.empty() && (wsubject.back() == L'-'))
  {
    wsubject.pop_back();
  }

  std::string filename = FileUtil::GetDownloadsDir() + std::to_string(uid) +
    (wsubject.empty() ? "" : ("-" + StrUtil::ToString(wsubject))) + (p_Markdown ? ".md" : ".txt");
  if (!PromptString("Save Filename: ", "Save", filename))
  {
    SetDialogMessage("Save cancelled");
    return;
  }

  if (filename.empty())
  {
    SetDialogMessage("Save cancelled (empty filename)");
    return;
  }

  filename = FileUtil::ExpandPath(filename);

  // view text is header block, blank line, body
  const std::string& viewText = m_CurrentMessageViewText;
  const size_t headerEnd = viewText.find("\n\n");
  const size_t bodyBegin = (headerEnd == std::string::npos) ? 0 : (headerEnd + 2);
  const std::string header = viewText.substr(0, bodyBegin);
  std::string body = viewText.substr(bodyBegin);
  if (hasMarkdown && !p_Markdown)
  {
    body = StrUtil::ToString(TextStyle::MarkdownToPlain(StrUtil::ToWString(body), m_MarkdownViewLinks));
  }

  FileUtil::WriteFile(filename, header + body);
  SetDialogMessage(p_Markdown ? "Markdown saved" : "Text saved");
}

void UiMessageView::MessageFind()
{
  std::string query = m_PersistFindQuery ? m_MessageFindQuery : "";
  if (PromptString("Find Text: ", "Find", query))
  {
    if (!query.empty() && (query != m_MessageFindQuery))
    {
      m_MessageFindMatchLine = -1;
      m_MessageFindQuery = query;
    }

    if (!m_MessageFindQuery.empty())
    {
      MessageFindNext();
    }
  }
}

void UiMessageView::MessageFindNext()
{
  int findFromLine = m_MessageFindMatchLine + 1;
  const std::wstring wquery = StrUtil::ToFold(StrUtil::ToWString(m_MessageFindQuery));
  std::vector<std::wstring> wlines = UiMessageView::GetCachedWordWrapLines("", 0);
  int countLines = wlines.size();

  bool found = false;
  for (int i = findFromLine; i < countLines; ++i)
  {
    std::vector<size_t> positions;
    std::wstring wline = StrUtil::ToFold(wlines.at(i), &positions);
    size_t pos = wline.find(wquery);
    if (!wquery.empty() && (pos != std::string::npos))
    {
      // map match in folded line back to original line
      const size_t matchFirst = positions.at(pos);
      const size_t matchLast = positions.at(pos + wquery.size() - 1);
      m_MessageViewLineOffset = StrUtil::Bound(0, i, countLines - m_MainWinHeight);
      m_MessageFindMatchLine = i;
      m_MessageFindMatchPos = matchFirst;
      m_MessageFindMatchLen = matchLast - matchFirst + 1;
      found = true;
      break;
    }
  }

  if (!found)
  {
    SetDialogMessage((m_MessageFindMatchLine == -1) ? "No matches found" : "No more matches found");
    m_MessageFindMatchLine = -1; // wrap around search
  }
  else
  {
    SetDialogMessage("");
  }
}

const std::vector<std::wstring>& UiMessageView::GetCachedWordWrapLines(const std::string& p_Folder, uint32_t p_Uid)
{
  static std::string prevFolder;
  static uint32_t prevUid = 0;
  static bool prevPlaintext = false;
  static bool prevProcessFlowed = false;
  static bool prevShowFullHeader = false;
  static bool prevShowCompactHeader = false;
  static bool prevFromHtml = false;
  static int32_t prevMaxViewLineLength = 0;
  static size_t prevTextLen = 0;

  std::vector<std::wstring>& wlines = m_MessageViewLines;
  if (p_Folder.empty() && (p_Uid == 0)) return wlines;

  if ((prevFolder == p_Folder) && (prevUid == p_Uid) && (prevPlaintext == m_Plaintext) &&
      (prevProcessFlowed == m_CurrentMessageProcessFlowed) && (prevShowFullHeader == m_ShowFullHeader) &&
      (prevShowCompactHeader == m_ShowCompactHeader) &&
      (prevFromHtml == m_CurrentMessageFromHtml) &&
      (prevMaxViewLineLength == m_MaxViewLineLength) && (prevTextLen == m_CurrentMessageViewText.size()))
  {
    return wlines;
  }

  prevFolder = p_Folder;
  prevUid = p_Uid;
  prevPlaintext = m_Plaintext;
  prevProcessFlowed = m_CurrentMessageProcessFlowed;
  prevShowFullHeader = m_ShowFullHeader;
  prevShowCompactHeader = m_ShowCompactHeader;
  prevFromHtml = m_CurrentMessageFromHtml;
  prevMaxViewLineLength = m_MaxViewLineLength;
  prevTextLen = m_CurrentMessageViewText.size(); // cater for search results async header load

  const std::wstring wtext = StrUtil::ToWString(m_CurrentMessageViewText);
  const bool outputFlowed = false; // only generate when sending after compose
  const bool quoteWrap = m_RewrapQuotedLines;
  const int expandTabSize = m_TabSize; // enabled

  // header values wrapped with hanging indent, when there is room for them
  wlines.clear();
  size_t bodyStart = 0;
  const size_t headerEnd = wtext.find(L"\n\n");
  const bool compactHeader = m_ShowCompactHeader && !m_ShowFullHeader && (headerEnd != std::wstring::npos);
  const bool hangingIndent = m_AlignHeaderLabels && !m_ShowFullHeader && !compactHeader &&
    (headerEnd != std::wstring::npos) && (m_MaxViewLineLength >= (int32_t)(HEADER_VALUE_COL + HEADER_VALUE_MIN_WIDTH));
  if (compactHeader)
  {
    // header lines truncated to fit
    std::wistringstream headerss(wtext.substr(0, headerEnd));
    std::wstring line;
    while (std::getline(headerss, line))
    {
      std::replace(line.begin(), line.end(), L'\t', L' ');
      wlines.push_back(StrUtil::TrimEllipsisWString(line, m_MaxViewLineLength));
    }

    bodyStart = headerEnd + 1; // keep blank line separating header and body
  }
  else if (hangingIndent)
  {
    const std::wstring indent(HEADER_VALUE_COL, L' ');
    const unsigned valueWidth = m_MaxViewLineLength - HEADER_VALUE_COL;
    std::wistringstream headerss(wtext.substr(0, headerEnd));
    std::wstring line;
    while (std::getline(headerss, line))
    {
      // lines without label, if any, indented as continuation
      const bool hasLabel = (line.size() >= HEADER_VALUE_COL) && (line.find(L':') < HEADER_VALUE_COL);
      const std::wstring prefix = hasLabel ? line.substr(0, HEADER_VALUE_COL) : indent;
      const std::wstring value = hasLabel ? line.substr(HEADER_VALUE_COL) : line;
      const std::vector<std::wstring> valueLines =
        TextUtil::WordWrap(value, valueWidth, false, outputFlowed, false, expandTabSize);
      if (valueLines.empty())
      {
        wlines.push_back(prefix);
        continue;
      }

      for (size_t i = 0; i < valueLines.size(); ++i)
      {
        wlines.push_back(((i == 0) ? prefix : indent) + valueLines.at(i));
      }
    }

    bodyStart = headerEnd + 1; // keep blank line separating header and body
  }

  const std::vector<std::wstring> bodyLines =
    TextUtil::WordWrap(wtext.substr(bodyStart), m_MaxViewLineLength, m_CurrentMessageProcessFlowed,
                       outputFlowed, quoteWrap, expandTabSize);
  wlines.insert(wlines.end(), bodyLines.begin(), bodyLines.end());
  wlines.push_back(L"");

  size_t wlinesSize = wlines.size();
  for (size_t i = 0; i < wlinesSize; ++i)
  {
    if (wlines[i].empty())
    {
      m_MessageViewHeaderLineCount = i;
      break;
    }
  }

  m_MessageViewSpans.assign(wlinesSize, std::vector<TextStyle::Span>());
  m_MessageViewLinks.clear();
  m_MessageViewLinksFolderUid = std::make_pair(p_Folder, (int32_t)p_Uid);
  m_MessageViewLinkIndex = -1;
  m_MessageViewLinkPastEnd = false;
  if (m_AttrsHeaderName != A_NORMAL)
  {
    TextStyle::StyleHeaderLines(wlines, m_MessageViewHeaderLineCount, m_AttrsHeaderName, m_ShowFullHeader,
                                m_MessageViewSpans);
  }

  // markdown only from built-in html converter, otherwise plain text urls
  if (m_MarkdownView && m_CurrentMessageFromHtml && ExtUtil::GetHtmlToTextConvertCmd().empty())
  {
    TextStyle::MarkdownAttrs attrs;
    attrs.m_Bold = m_AttrsBoldText;
    attrs.m_Italic = m_AttrsItalicText;
    attrs.m_Code = m_AttrsCodeText;
    attrs.m_Strike = m_AttrsStrikeText;
    attrs.m_Link = m_AttrsLinkText;
    TextStyle::StyleMarkdownLines(wlines, m_MessageViewHeaderLineCount + 1, attrs, m_MessageViewSpans,
                                  m_MarkdownViewLinks ? &m_MessageViewLinks : nullptr);
  }
  else if (m_PlainViewLinks)
  {
    TextStyle::StyleUrlLines(wlines, m_MessageViewHeaderLineCount + 1, m_AttrsLinkText, m_MaxViewLineLength,
                             m_MessageViewSpans, m_MessageViewLinks);
  }

  if (m_AttrsSignatureText != A_NORMAL)
  {
    TextStyle::StyleSignatureLines(wlines, m_MessageViewHeaderLineCount + 1, m_AttrsSignatureText,
                                   m_MessageViewSpans);
  }

  return wlines;
}

// header label in compose style, e.g. "To      : ", or unaligned "To: "
std::string UiMessageView::GetHeaderLabel(const std::string& p_Name) const
{
  if (!m_AlignHeaderLabels) return p_Name + ": ";

  const size_t nameWidth = HEADER_VALUE_COL - 2;
  return p_Name + std::string((p_Name.size() < nameWidth) ? (nameWidth - p_Name.size()) : 0, ' ') + ": ";
}

std::string UiMessageView::GetBodyText(Body& p_Body)
{
  if (!m_Plaintext)
  {
    if (p_Body.ParseHtmlIfNeeded())
    {
      const std::string& folder = m_CurrentFolderUid.first;
      const int uid = m_CurrentFolderUid.second;

      ImapManager::Action imapAction;
      imapAction.m_Folder = folder;
      imapAction.m_UpdateCache = true;
      imapAction.m_SetBodysCache[uid] = p_Body;
      m_Model->GetImapManager()->AsyncAction(imapAction);
    }
  }
  // skip leading blank lines, keeping indent of first line
  const std::string& text = m_Plaintext ? p_Body.GetTextPlain() : p_Body.GetTextHtml();
  const size_t contentPos = text.find_first_not_of(" \t\r\n");
  const size_t lineEnd = (contentPos == std::string::npos) ? std::string::npos : text.rfind('\n', contentPos);
  return (lineEnd == std::string::npos) ? text : text.substr(lineEnd + 1);
}

void UiMessageView::OnEnter()
{
  curs_set(0);
  m_MessageViewLineOffset = 0;
  m_HelpView->ResetMessageOffset();
  m_MessageFindMatchLine = -1;
  m_MessageViewLinkIndex = -1;
  m_MessageViewLinkPastEnd = false;
}
