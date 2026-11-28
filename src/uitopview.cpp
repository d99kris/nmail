// uitopview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <cstdint>

#include "flag.h"
#include "strutil.h"
#include "version.h"

void Ui::DrawTop()
{
  werase(m_TopWin);
  wattron(m_TopWin, m_AttrsTopBar);

  static const std::string appName = "  " + Version::GetAppName(m_TopBarShowVersion);
  std::string topLeft = StrUtil::TrimPadString(appName, (m_ScreenWidth - 13) / 2);
  std::string status = GetStatusStr();
  std::string topRight = status + "  ";
  int centerWidth = m_ScreenWidth - (int)topLeft.size() - (int)topRight.size() - 2;
  std::wstring wtopCenter = StrUtil::TrimPadWString(StrUtil::ToWString(GetStateStr()), centerWidth) + L"  ";
  std::string topCenter = StrUtil::ToString(wtopCenter);
  std::string topCombined = topLeft + topCenter + topRight;

  mvwprintw(m_TopWin, 0, 0, "%s", topCombined.c_str());
  wattroff(m_TopWin, m_AttrsTopBar);
  RefreshWin(m_TopWin);
}

std::string Ui::GetStatusStr()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.ToString();
}

std::string Ui::GetStateStr()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  switch (m_State)
  {
    case StateViewMessageList:
      if (m_MessageListSearch)
      {
        return "Search: " + m_MessageListSearchQuery;
      }
      else if (m_SortFilter[m_CurrentFolder] != SortDefault)
      {
        return "Folder: " + m_CurrentFolder + GetCountStateStr() + GetFilterStateStr();
      }
      else
      {
        return "Folder: " + m_CurrentFolder + GetCountStateStr();
      }
    case StateViewMessage:
      {
        std::string str = std::string("Message ") + (m_Plaintext ? "plain" : "html");
        if (m_MessageViewToggledSeen)
        {
          const std::map<uint32_t, uint32_t>& flags = m_Flags[m_CurrentFolder];
          const int uid = m_MessageListCurrentUid[m_CurrentFolder];
          const bool unread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
          if (unread)
          {
            str += " [unread]";
          }
        }

        return str;
      }
    case StateGotoFolder:
      return "Goto Folder";
    case StateMoveToFolder:
      return "Move To Folder";
    case StateComposeMessage:
      return std::string("Compose") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateComposeCopyMessage:
      return std::string("Compose Copy") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateReplyAllMessage:
    case StateReplySenderMessage:
      return std::string("Reply") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateForwardMessage:
      return std::string("Forward") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateForwardAttachedMessage:
      return std::string("Forward Attached") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateAddressList:
    case StateFromAddressList:
      return "Address Book";
    case StateFileList:
      return "File Selection";
    case StateViewPartList:
      return "Message Parts";
    default: return "Unknown State";
  }
}

std::string Ui::GetCountStateStr()
{
  if (!m_TopBarShowMessageCount) return "";

  const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(m_CurrentFolder);
  return " (" + std::to_string(displayUids.size()) + ")";
}

std::string Ui::GetFilterStateStr()
{
  switch (m_SortFilter[m_CurrentFolder])
  {
    case SortUnseenAsc:
      return " [Unrd Asc]";
    case SortUnseenDesc:
      return " [Unrd Desc]";
    case SortUnseenOnly:
      return " [Unrd Only]";
    case SortAttchAsc:
      return " [Attc Asc]";
    case SortAttchDesc:
      return " [Attc Desc]";
    case SortAttchOnly:
      return " [Attc Only]";
    case SortDateAsc:
      return " [Date Asc]";
    case SortDateDesc:
      return " [Date Desc]";
    case SortCurrDateOnly:
      return " [Date Curr]";
    case SortNameAsc:
      return " [Name Asc]";
    case SortNameDesc:
      return " [Name Desc]";
    case SortCurrNameOnly:
      return " [Name Curr]";
    case SortSubjAsc:
      return " [Subj Asc]";
    case SortSubjDesc:
      return " [Subj Desc]";
    case SortCurrSubjOnly:
      return " [Subj Curr]";
    default:
      return ""; // should not reach here
  }
}
