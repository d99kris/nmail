// uitopview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uitopview.h"

#include <cstdint>

#include "flag.h"
#include "strutil.h"
#include "uiconfig.h"
#include "version.h"

UiTopView::UiTopView(Ui* p_Ui)
  : UiViewBase(p_Ui)
{
  m_TopBarShowVersion = UiConfig::GetBool("top_bar_show_version");
  m_TopBarShowMessageCount = UiConfig::GetBool("top_bar_show_message_count");
}

void UiTopView::Draw()
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

std::string UiTopView::GetStatusStr()
{
  UiModel::Lock model(m_Model);
  return model->m_Status.ToString();
}

std::string UiTopView::GetStateStr()
{
  UiModel::Lock model(m_Model);

  switch (m_State)
  {
    case Ui::StateViewMessageList:
      if (m_MessageListSearch)
      {
        return "Search: " + m_MessageListSearchQuery;
      }
      else if (model->m_SortFilter[m_CurrentFolder] != Ui::SortDefault)
      {
        return "Folder: " + m_CurrentFolder + GetCountStateStr(model) + GetFilterStateStr(model);
      }
      else
      {
        return "Folder: " + m_CurrentFolder + GetCountStateStr(model);
      }
    case Ui::StateViewMessage:
      {
        std::string str = std::string("Message ") + (m_Plaintext ? "plain" : "html");
        if (m_MessageViewToggledSeen)
        {
          const std::map<uint32_t, uint32_t>& flags = model->m_Flags[m_CurrentFolder];
          const int uid = m_MessageListCurrentUid[m_CurrentFolder];
          const bool unread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
          if (unread)
          {
            str += " [unread]";
          }
        }

        return str;
      }
    case Ui::StateGotoFolder:
      return "Goto Folder";
    case Ui::StateMoveToFolder:
      return "Move To Folder";
    case Ui::StateComposeMessage:
      return std::string("Compose") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case Ui::StateComposeCopyMessage:
      return std::string("Compose Copy") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case Ui::StateReplyAllMessage:
    case Ui::StateReplySenderMessage:
      return std::string("Reply") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case Ui::StateForwardMessage:
      return std::string("Forward") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case Ui::StateForwardAttachedMessage:
      return std::string("Forward Attached") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case Ui::StateAddressList:
    case Ui::StateFromAddressList:
      return "Address Book";
    case Ui::StateFileList:
      return "File Selection";
    case Ui::StateViewPartList:
      return "Message Parts";
    default: return "Unknown Ui::State";
  }
}

std::string UiTopView::GetCountStateStr(UiModel::Lock& p_Model)
{
  if (!m_TopBarShowMessageCount) return "";

  const std::map<std::string, uint32_t>& displayUids = p_Model->GetDisplayUids(m_CurrentFolder);
  return " (" + std::to_string(displayUids.size()) + ")";
}

std::string UiTopView::GetFilterStateStr(UiModel::Lock& p_Model)
{
  switch (p_Model->m_SortFilter[m_CurrentFolder])
  {
    case Ui::SortUnseenAsc:
      return " [Unrd Asc]";
    case Ui::SortUnseenDesc:
      return " [Unrd Desc]";
    case Ui::SortUnseenOnly:
      return " [Unrd Only]";
    case Ui::SortAttchAsc:
      return " [Attc Asc]";
    case Ui::SortAttchDesc:
      return " [Attc Desc]";
    case Ui::SortAttchOnly:
      return " [Attc Only]";
    case Ui::SortDateAsc:
      return " [Date Asc]";
    case Ui::SortDateDesc:
      return " [Date Desc]";
    case Ui::SortCurrDateOnly:
      return " [Date Curr]";
    case Ui::SortNameAsc:
      return " [Name Asc]";
    case Ui::SortNameDesc:
      return " [Name Desc]";
    case Ui::SortCurrNameOnly:
      return " [Name Curr]";
    case Ui::SortSubjAsc:
      return " [Subj Asc]";
    case Ui::SortSubjDesc:
      return " [Subj Desc]";
    case Ui::SortCurrSubjOnly:
      return " [Subj Curr]";
    default:
      return ""; // should not reach here
  }
}
