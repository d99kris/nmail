// uihelpview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>

#include "strutil.h"

void Ui::DrawHelp()
{
  static std::vector<std::vector<std::string>> viewMessagesListCommonHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "Folders",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReplyAll), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyRefresh), "Refresh",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewMsg",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyAutoMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
    {
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyExport), "Export",
      GetKeyDisplay(m_KeyImport), "Import",
      GetKeyDisplay(m_KeySearch), "Search",
      GetKeyDisplay(m_KeySync), "FullSync",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyExtHtmlViewer), "ExtVHtml",
      GetKeyDisplay(m_KeyExtMsgViewer), "ExtVMsg",
      GetKeyDisplay(m_KeySelectAll), "SelectAll",
      GetKeyDisplay(m_KeySearchServer), "SearchSrv",
      GetKeyDisplay(m_KeySearchCurrentName), "SearchNam",
      GetKeyDisplay(m_KeySearchCurrentSubject), "SearchSub",
    },
    {
      GetKeyDisplay(m_KeyGotoInbox), "GotoInbox",
      "", "",
      "", "",
      "", "",
      "", "",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      "", "",
      "", "",
      "", "",
      "", "",
      "", "",
      "", "",
    },
  };

  static std::vector<std::vector<std::string>> viewMessagesListHelp = [&]()
  {
    std::vector<std::vector<std::string>> listHelp = viewMessagesListCommonHelp;
    listHelp.push_back(
    {
      GetKeyDisplay(m_KeySortUnread), "SortUnrd",
      GetKeyDisplay(m_KeySortHasAttachments), "SortAttc",
      GetKeyDisplay(m_KeySortDate), "SortDate",
      GetKeyDisplay(m_KeySortName), "SortName",
      GetKeyDisplay(m_KeySortSubject), "SortSubj",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    });
    listHelp.push_back(
    {
      GetKeyDisplay(m_KeyFilterShowUnread), "FiltUnrd",
      GetKeyDisplay(m_KeyFilterShowHasAttachments), "FiltAttc",
      GetKeyDisplay(m_KeyFilterShowCurrentDate), "FiltDate",
      GetKeyDisplay(m_KeyFilterShowCurrentName), "FiltName",
      GetKeyDisplay(m_KeyFilterShowCurrentSubject), "FiltSubj",
      GetKeyDisplay(m_KeyFilterSortReset), "FiltReset",
    });
    return listHelp;
  }();

  static std::vector<std::vector<std::string>> viewMessagesListSearchHelp = [&]()
  {
    std::vector<std::vector<std::string>> listHelp = viewMessagesListCommonHelp;
    listHelp[0][1] = "MsgList"; // instead of "Folders"

    listHelp.push_back(
    {
      GetKeyDisplay(m_KeyJumpTo), "JumpTo",
      "", "",
      "", "",
      "", "",
      "", "",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    });
    listHelp.push_back(
    {
      GetKeyDisplay(m_KeySearchShowFolder), "ShowFold",
    });

    return listHelp;
  }();

  static std::vector<std::vector<std::string>> viewMessageHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "MsgList",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReplyAll), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleTextHtml), "TgTxtHtml",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyOpen), "MsgParts",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyAutoMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
    {
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyExport), "Export",
      GetKeyDisplay(m_KeyExtPager), "ExtPager",
      GetKeyDisplay(m_KeyExtHtmlViewer), "ExtVHtml",
      GetKeyDisplay(m_KeyExtMsgViewer), "ExtVMsg",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyFind), "Find",
      GetKeyDisplay(m_KeyFindNext), "FindNext",
      GetKeyDisplay(m_KeyToggleFullHeader), "TgFullHdr",
      GetKeyDisplay(m_KeyGotoInbox), "GotoInbox",
      GetKeyDisplay(m_KeyNextLink), "NextLink",
      GetKeyDisplay(m_KeyOpenLink), "OpenLink",
    },
    {
      GetKeyDisplay(m_KeySaveFile), "Save",
      "", "",
      "", "",
      "", "",
      "", "",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      "", "",
      "", "",
      "", "",
      "", "",
      "", "",
      "", "",
    },
  };

  static std::vector<std::vector<std::string>> viewFoldersHelp =
  {
    {
      GetKeyDisplay(m_KeyReturn), "Select",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
    }
  };

  static std::vector<std::vector<std::string>> composeMessageHelp =
  {
    {
      GetKeyDisplay(m_KeySend), "Send",
      GetKeyDisplay(m_KeyExtEditor), "ExtEdit",
      GetKeyDisplay(m_KeyRichHeader), "RichHdr",
      GetKeyDisplay(m_KeyToggleMarkdownCompose), "TgMkDown",
      GetKeyDisplay(m_KeySpell), "Spell",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
      GetKeyDisplay(m_KeyPostpone), "Postpone",
      GetKeyDisplay(m_KeyToSelect), "ToSelect",
      GetKeyDisplay(m_KeyExtHtmlPreview), "ExtVHtml",
    },
  };

  static std::vector<std::vector<std::string>> viewPartListHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "ViewMsg",
      GetKeyDisplay(m_KeyPrevMsg), "PrevPart",
      GetKeyDisplay(m_KeySaveFile), "Save",
      GetKeyDisplay(m_KeyExtHtmlViewer), "ExtVHtml",
      GetKeyDisplay(m_KeyGotoInbox), "GotoInbox",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewPart",
      GetKeyDisplay(m_KeyNextMsg), "NextPart",
      GetKeyDisplay(m_KeySaveAllFiles), "SaveAll",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
  };

  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    switch (m_State)
    {
      case StateViewMessageList:
        {
          const std::vector<std::vector<std::string>>& listHelp =
            m_MessageListSearch ? viewMessagesListSearchHelp : viewMessagesListHelp;
          m_HelpViewMessagesListSize = listHelp.size();

          auto first = listHelp.begin() + m_HelpViewMessagesListOffset;
          auto last = listHelp.begin() + m_HelpViewMessagesListOffset + 2;
          std::vector<std::vector<std::string>> helpMessages(first, last);
          DrawHelpText(helpMessages);
          break;
        }

      case StateViewMessage:
        {
          m_HelpViewMessageSize = viewMessageHelp.size();
          auto first = viewMessageHelp.begin() + m_HelpViewMessageOffset;
          auto last = viewMessageHelp.begin() + m_HelpViewMessageOffset + 2;
          std::vector<std::vector<std::string>> helpMessages(first, last);
          DrawHelpText(helpMessages);
          break;
        }
        break;

      case StateGotoFolder:
      case StateMoveToFolder:
      case StateAddressList:
      case StateFromAddressList:
      case StateFileList:
        DrawHelpText(viewFoldersHelp, 2);
        break;

      case StateComposeMessage:
      case StateComposeCopyMessage:
      case StateReplyAllMessage:
      case StateReplySenderMessage:
      case StateForwardMessage:
      case StateForwardAttachedMessage:
        DrawHelpText(composeMessageHelp, 2);
        break;

      case StateViewPartList:
        DrawHelpText(viewPartListHelp);
        break;

      default:
        break;
    }

    RefreshWin(m_HelpWin);
  }
}

void Ui::DrawHelpText(const std::vector<std::vector<std::string>>& p_HelpText, int p_KeyWidth)
{
  int cols = 6;
  int width = m_ScreenWidth / cols;

  int y = 0;
  for (auto rowIt = p_HelpText.begin(); rowIt != p_HelpText.end(); ++rowIt)
  {
    int x = 0;
    for (int colIdx = 0; colIdx < (int)rowIt->size(); colIdx += 2)
    {
      std::wstring wcmd = StrUtil::ToWString(rowIt->at(colIdx));
      std::wstring wdesc = StrUtil::ToWString(rowIt->at(colIdx + 1));

      // right-align key to min width
      const int cmdWidth = StrUtil::WStringWidth(wcmd);
      const int cmdPad = std::max(0, p_KeyWidth - cmdWidth);
      const int cmdX = x + cmdPad;

      wattron(m_HelpWin, m_AttrsHelpKeys);
      mvwaddnwstr(m_HelpWin, y, cmdX, wcmd.c_str(), wcmd.size());
      wattroff(m_HelpWin, m_AttrsHelpKeys);

      wattron(m_HelpWin, m_AttrsHelpDesc);
      const std::wstring wdescTrim = wdesc.substr(0, std::max(0, width - cmdPad - cmdWidth - 2));
      mvwaddnwstr(m_HelpWin, y, cmdX + cmdWidth + 1, wdescTrim.c_str(), wdescTrim.size());
      wattroff(m_HelpWin, m_AttrsHelpDesc);

      x += width;
    }

    ++y;
  }
}

std::string Ui::GetKeyDisplay(int p_Key)
{
  if (p_Key == '\n')
  {
    return "\xE2\x8F\x8E"; // return symbol
  }
  else if (p_Key == '\t')
  {
    return "\xE2\x87\xA5"; // tab symbol
  }
  else if ((p_Key >= 0x0) && (p_Key <= 0x1F))
  {
    return "^" + std::string(1, (char)p_Key + 0x40);
  }
  else if (p_Key == ',')
  {
    return "<";
  }
  else if (p_Key == '.')
  {
    return ">";
  }
  else if (p_Key == m_KeyLeft)
  {
    return "←";
  }
  else if (p_Key == m_KeyRight)
  {
    return "→";
  }
  else if ((p_Key >= 'a') && (p_Key <= 'z'))
  {
    return std::string(1, std::toupper((char)p_Key));
  }
  else
  {
    return std::string(1, (char)p_Key);
  }

  return "??";
}
