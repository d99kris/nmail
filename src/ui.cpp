// ui.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <sstream>

#include "addressbook.h"
#include "flag.h"
#include "loghelp.h"
#include "maphelp.h"
#include "sethelp.h"
#include "status.h"

Ui::Ui(const std::string& p_Inbox, const std::string& p_Address, uint32_t p_PrefetchLevel)
  : m_Inbox(p_Inbox)
  , m_Address(p_Address)
  , m_PrefetchLevel(p_PrefetchLevel)
{
  m_CurrentFolder = p_Inbox;
  Init();
  InitWindows();
}

Ui::~Ui()
{
  CleanupWindows();
  Cleanup();
}

void Ui::Init()
{
  signal(SIGINT, SIG_IGN);
  setlocale(LC_ALL, "");
  initscr();
  noecho();
  cbreak();
  raw();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(0);
  pipe(m_Pipe);

  const std::map<std::string, std::string> defaultConfig =
  {
    { "compose_hardwrap", "0" },
    { "help_enabled", "1" },
    { "persist_folder_filter", "1" },
    { "persist_search_query", "0" },
    { "plain_text", "1" },
    { "show_progress", "1" },
    { "new_msg_bell", "1" },
    { "quit_without_confirm", "1" },
    { "send_without_confirm", "0" },
    { "cancel_without_confirm", "0" },
    { "postpone_without_confirm", "0" },
    { "delete_without_confirm", "0" },
    { "show_embedded_images", "1" },
    { "show_rich_header", "0" },
    { "markdown_html_compose", "0" },
    { "key_prev_msg", "p" },
    { "key_next_msg", "n" },
    { "key_reply", "r" },
    { "key_forward", "f" },
    { "key_delete", "d" },
    { "key_compose", "c" },
    { "key_toggle_unread", "u" },
    { "key_move", "m" },
    { "key_refresh", "l" },
    { "key_quit", "q" },
    { "key_toggle_text_html", "t" },
    { "key_cancel", "KEY_CTRLC" },
    { "key_send", "KEY_CTRLX" },
    { "key_delete_line", "KEY_CTRLK" },
    { "key_open", "." },
    { "key_back", "," },
    { "key_goto_folder", "g" },
    { "key_to_select", "KEY_CTRLT" },
    { "key_save_file", "s" },
    { "key_ext_editor", "KEY_CTRLE" },
    { "key_ext_pager", "e" },
    { "key_postpone", "KEY_CTRLO" },
    { "key_othercmd_help", "o" },
    { "key_export", "x" },
    { "key_import", "i" },
    { "key_rich_header", "KEY_CTRLR" },
    { "key_ext_html_viewer", "v" },
    { "key_ext_html_preview", "KEY_CTRLV" },
    { "key_ext_msg_viewer", "w" },
    { "key_search", "/" },
    { "key_sync", "s" },
    { "key_toggle_markdown_compose", "KEY_CTRLN" },
#if defined(__APPLE__)
    { "key_backward_word", "\\033\\142" }, // opt-left
    { "key_forward_word", "\\033\\146" }, // opt-right
    { "key_backward_kill_word", "\\033\\177" }, // opt-backspace
    { "key_kill_word", "\\033\\010" }, // opt-delete
#else // defined(__linux__)
    { "key_backward_word", "0x21f" }, // alt-left
    { "key_forward_word", "0x22e" }, // alt-right
    { "key_backward_kill_word", "\\033\\177" }, // alt-backspace
    { "key_kill_word", "0x205" }, // alt-delete
#endif
    { "key_prev_page", "KEY_PPAGE" },
    { "key_next_page", "KEY_NPAGE" },
    { "colors_enabled", "0" },
    { "attachment_indicator", "+" },
  };
  const std::string configPath(Util::GetApplicationDir() + std::string("ui.conf"));
  m_Config = Config(configPath, defaultConfig);

  m_ComposeHardwrap = m_Config.Get("compose_hardwrap") == "1";
  m_HelpEnabled = m_Config.Get("help_enabled") == "1";
  m_PersistFolderFilter = m_Config.Get("persist_folder_filter") == "1";
  m_PersistSearchQuery = m_Config.Get("persist_search_query") == "1";
  m_Plaintext = m_Config.Get("plain_text") == "1";
  m_MarkdownHtmlCompose = m_Config.Get("markdown_html_compose") == "1";
  m_KeyPrevMsg = Util::GetKeyCode(m_Config.Get("key_prev_msg"));
  m_KeyNextMsg = Util::GetKeyCode(m_Config.Get("key_next_msg"));
  m_KeyReply = Util::GetKeyCode(m_Config.Get("key_reply"));
  m_KeyForward = Util::GetKeyCode(m_Config.Get("key_forward"));
  m_KeyDelete = Util::GetKeyCode(m_Config.Get("key_delete"));
  m_KeyCompose = Util::GetKeyCode(m_Config.Get("key_compose"));
  m_KeyToggleUnread = Util::GetKeyCode(m_Config.Get("key_toggle_unread"));
  m_KeyMove = Util::GetKeyCode(m_Config.Get("key_move"));
  m_KeyRefresh = Util::GetKeyCode(m_Config.Get("key_refresh"));
  m_KeyQuit = Util::GetKeyCode(m_Config.Get("key_quit"));
  m_KeyToggleTextHtml = Util::GetKeyCode(m_Config.Get("key_toggle_text_html"));
  m_KeyCancel = Util::GetKeyCode(m_Config.Get("key_cancel"));
  m_KeySend = Util::GetKeyCode(m_Config.Get("key_send"));
  m_KeyDeleteLine = Util::GetKeyCode(m_Config.Get("key_delete_line"));
  m_KeyOpen = Util::GetKeyCode(m_Config.Get("key_open"));
  m_KeyBack = Util::GetKeyCode(m_Config.Get("key_back"));
  m_KeyGotoFolder = Util::GetKeyCode(m_Config.Get("key_goto_folder"));
  m_KeyToSelect = Util::GetKeyCode(m_Config.Get("key_to_select"));
  m_KeySaveFile = Util::GetKeyCode(m_Config.Get("key_save_file"));
  m_KeyExtEditor = Util::GetKeyCode(m_Config.Get("key_ext_editor"));
  m_KeyExtPager = Util::GetKeyCode(m_Config.Get("key_ext_pager"));
  m_KeyPostpone = Util::GetKeyCode(m_Config.Get("key_postpone"));
  m_KeyOtherCmdHelp = Util::GetKeyCode(m_Config.Get("key_othercmd_help"));
  m_KeyExport = Util::GetKeyCode(m_Config.Get("key_export"));
  m_KeyImport = Util::GetKeyCode(m_Config.Get("key_import"));
  m_KeyRichHeader = Util::GetKeyCode(m_Config.Get("key_rich_header"));
  m_KeyExtHtmlViewer = Util::GetKeyCode(m_Config.Get("key_ext_html_viewer"));
  m_KeyExtHtmlPreview = Util::GetKeyCode(m_Config.Get("key_ext_html_preview"));
  m_KeyExtMsgViewer = Util::GetKeyCode(m_Config.Get("key_ext_msg_viewer"));
  m_KeySearch = Util::GetKeyCode(m_Config.Get("key_search"));
  m_KeySync = Util::GetKeyCode(m_Config.Get("key_sync"));
  m_KeyToggleMarkdownCompose = Util::GetKeyCode(m_Config.Get("key_toggle_markdown_compose"));

  m_KeyBackwardWord = Util::GetKeyCode(m_Config.Get("key_backward_word"));
  m_KeyForwardWord = Util::GetKeyCode(m_Config.Get("key_forward_word"));
  m_KeyBackwardKillWord = Util::GetKeyCode(m_Config.Get("key_backward_kill_word"));
  m_KeyKillWord = Util::GetKeyCode(m_Config.Get("key_kill_word"));

  m_KeyPrevPage = Util::GetKeyCode(m_Config.Get("key_prev_page"));
  m_KeyNextPage = Util::GetKeyCode(m_Config.Get("key_next_page"));

  m_ShowProgress = m_Config.Get("show_progress") == "1";
  m_NewMsgBell = m_Config.Get("new_msg_bell") == "1";
  m_QuitWithoutConfirm = m_Config.Get("quit_without_confirm") == "1";
  m_SendWithoutConfirm = m_Config.Get("send_without_confirm") == "1";
  m_CancelWithoutConfirm = m_Config.Get("cancel_without_confirm") == "1";
  m_PostponeWithoutConfirm = m_Config.Get("postpone_without_confirm") == "1";
  m_DeleteWithoutConfirm = m_Config.Get("delete_without_confirm") == "1";
  m_ShowEmbeddedImages = m_Config.Get("show_embedded_images") == "1";
  m_ShowRichHeader = m_Config.Get("show_rich_header") == "1";

  m_ColorsEnabled = m_Config.Get("colors_enabled") == "1";
  if (m_ColorsEnabled)
  {
    if (!has_colors())
    {
      LOG_WARNING("terminal does not support colors");
      m_ColorsEnabled = false;
    }
    else if (!can_change_color())
    {
      LOG_WARNING("terminal does not support changing colors");
      m_ColorsEnabled = false;
    }
  }

  if (m_ColorsEnabled)
  {
    start_color();
    assume_default_colors(-1, -1);

    const std::map<std::string, std::string> defaultColorsConfig =
    {
      { "color_message_quoted_fg", "0xa0a0a0" },
    };
    const std::string colorsConfigPath(Util::GetApplicationDir() + std::string("colors.conf"));
    Config colorsConfig = Config(colorsConfigPath, defaultColorsConfig);

    m_ColorMessageQuoted = Util::AddColorPair(colorsConfig.Get("color_message_quoted_fg"), "");

    colorsConfig.Save();
  }

  m_AttachmentIndicator = m_Config.Get("attachment_indicator");

  m_Running = true;
}

void Ui::Cleanup()
{
  m_Config.Set("plain_text", m_Plaintext ? "1" : "0");
  m_Config.Set("show_rich_header", m_ShowRichHeader ? "1" : "0");
  m_Config.Save();
  close(m_Pipe[0]);
  close(m_Pipe[1]);
  wclear(stdscr);
  endwin();
}

void Ui::InitWindows()
{
  getmaxyx(stdscr, m_ScreenHeight, m_ScreenWidth);
  m_ScreenWidth = std::max(m_ScreenWidth, 40);
  m_ScreenHeight = std::max(m_ScreenHeight, 8);

  m_MaxLineLength = m_ScreenWidth;
  wclear(stdscr);
  wrefresh(stdscr);
  const int topHeight = 1;
  m_TopWin = newwin(topHeight, m_ScreenWidth, 0, 0);
  leaveok(m_TopWin, true);

  int helpHeight = 0;
  if (m_HelpEnabled)
  {
    helpHeight = 2;
    m_HelpWin = newwin(2, m_ScreenWidth, m_ScreenHeight - helpHeight, 0);
    leaveok(m_HelpWin, true);
  }

  const int dialogHeight = 1;
  m_DialogWin = newwin(1, m_ScreenWidth, m_ScreenHeight - helpHeight - dialogHeight, 0);
  leaveok(m_DialogWin, true);

  bool listPad = true;
  if (listPad)
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight - 2;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight + 1, 0);
  }
  else
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight, 0);
  }

  leaveok(m_MainWin, true);
}

void Ui::CleanupWindows()
{
  delwin(m_TopWin);
  m_TopWin = NULL;
  delwin(m_MainWin);
  m_MainWin = NULL;
  delwin(m_DialogWin);
  m_DialogWin = NULL;
  if (m_HelpWin != NULL)
  {
    delwin(m_HelpWin);
    m_HelpWin = NULL;
  }
}

void Ui::DrawAll()
{
  switch (m_State)
  {
    case StateViewMessageList:
      DrawTop();
      if (m_MessageListSearch)
      {
        DrawMessageListSearch();
      }
      else
      {
        DrawMessageList();
      }
      DrawHelp();
      DrawDialog();
      break;

    case StateViewMessage:
      DrawTop();
      DrawMessage();
      DrawHelp();
      DrawDialog();
      break;

    case StateGotoFolder:
    case StateMoveToFolder:
      DrawTop();
      DrawFolderList();
      DrawHelp();
      DrawDialog();
      break;

    case StateComposeMessage:
    case StateReplyMessage:
    case StateForwardMessage:
      DrawTop();
      DrawHelp();
      DrawDialog();
      DrawComposeMessage();
      break;

    case StateAddressList:
      DrawTop();
      DrawAddressList();
      DrawHelp();
      DrawDialog();
      break;

    case StateFileList:
      DrawTop();
      DrawFileList();
      DrawHelp();
      DrawDialog();
      break;

    case StateViewPartList:
      DrawTop();
      DrawPartList();
      DrawHelp();
      DrawDialog();
      break;

    default:
      werase(m_MainWin);
      mvwprintw(m_MainWin, 0, 0, "Unimplemented state %d", m_State);
      wrefresh(m_MainWin);
      break;
  }
}

void Ui::DrawTop()
{
  werase(m_TopWin);
  wattron(m_TopWin, A_REVERSE);

  std::string version = "  nmail " + Util::GetAppVersion();
  std::string topLeft = Util::TrimPadString(version, (m_ScreenWidth - 13) / 2);
  std::string status = GetStatusStr();
  std::string topRight = status + "  ";
  std::string topCenter = Util::TrimPadString(GetStateStr(),
                                              m_ScreenWidth - topLeft.size() - topRight.size());
  std::string topCombined = topLeft + topCenter + topRight;

  mvwprintw(m_TopWin, 0, 0, "%s", topCombined.c_str());
  wattroff(m_TopWin, A_NORMAL);
  wrefresh(m_TopWin);
}

void Ui::DrawDialog()
{
  switch (m_State)
  {
    case StateGotoFolder:
    case StateMoveToFolder:
    case StateAddressList:
    case StateFileList:
      DrawSearchDialog();
      break;

    default:
      DrawDefaultDialog();
      break;
  }
}

void Ui::DrawSearchDialog()
{
  int filterPos = 0;
  std::wstring filterStr;

  switch (m_State)
  {
    case StateGotoFolder:
    case StateMoveToFolder:
      filterPos = m_FolderListFilterPos;
      filterStr = m_FolderListFilterStr;
      break;

    case StateAddressList:
      filterPos = m_AddressListFilterPos;
      filterStr = m_AddressListFilterStr;
      break;

    case StateFileList:
      filterPos = m_FileListFilterPos;
      filterStr = m_FileListFilterStr;
      break;

    default:
      break;
  }

  werase(m_DialogWin);
  const std::string& dispStr = Util::ToString(filterStr);
  mvwprintw(m_DialogWin, 0, 0, "   Search: %s", dispStr.c_str());

  leaveok(m_DialogWin, false);
  wmove(m_DialogWin, 0, 11 + filterPos);
  wrefresh(m_DialogWin);
  leaveok(m_DialogWin, true);
}

void Ui::DrawDefaultDialog()
{
  werase(m_DialogWin);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::chrono::time_point<std::chrono::system_clock> nowTime =
      std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = nowTime - m_DialogMessageTime;
    if ((elapsed.count() < 1.0f) && !m_DialogMessage.empty())
    {
      int x = std::max((m_ScreenWidth - (int)m_DialogMessage.size() - 1) / 2, 0);
      const std::string& dispStr = m_DialogMessage;
      wattron(m_DialogWin, A_REVERSE);
      mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
      wattroff(m_DialogWin, A_REVERSE);
    }
  }

  wrefresh(m_DialogWin);
}

void Ui::SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn /*= false */)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_DialogMessage = p_DialogMessage;
  m_DialogMessageTime = std::chrono::system_clock::now();
  if (!p_DialogMessage.empty())
  {
    const std::string& logMessage = Util::ToLower(p_DialogMessage);
    if (p_Warn)
    {
      LOG_WARNING("%s", logMessage.c_str());
    }
    else
    {
      LOG_DEBUG("%s", logMessage.c_str());
    }
  }
}

void Ui::DrawHelp()
{
  // @todo: non-static is not optimal performance-wise, consider separate static vectors here and select below
  std::vector<std::vector<std::string>> viewMessagesListHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), m_MessageListSearch ? "MsgList" : "Folders",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReply), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyRefresh), "Refresh",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewMsg",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyMove), "Move",
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
    },
  };

  static std::vector<std::vector<std::string>> viewMessageHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "MsgList",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReply), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleTextHtml), "TgTxtHtml",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyOpen), "MsgParts",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyMove), "Move",
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
    },
  };

  static std::vector<std::vector<std::string>> viewFoldersHelp =
  {
    {
      GetKeyDisplay(KEY_RETURN), "Select",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
    }
  };

  static std::vector<std::vector<std::string>> composeMessageHelp =
  {
    {
      GetKeyDisplay(m_KeySend), "Send",
      GetKeyDisplay(m_KeyDeleteLine), "DelLine",
      GetKeyDisplay(m_KeyExtEditor), "ExtEdit",
      GetKeyDisplay(m_KeyRichHeader), "RichHdr",
      GetKeyDisplay(m_KeyToggleMarkdownCompose), "TgMkDown",
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
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewPart",
      GetKeyDisplay(m_KeyNextMsg), "NextPart",
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
          auto first = viewMessagesListHelp.begin() + m_HelpViewMessagesListOffset;
          auto last = viewMessagesListHelp.begin() + m_HelpViewMessagesListOffset + 2;
          std::vector<std::vector<std::string>> helpMessages(first, last);
          DrawHelpText(helpMessages);
          break;
        }

      case StateViewMessage:
        {
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
      case StateFileList:
        DrawHelpText(viewFoldersHelp);
        break;

      case StateComposeMessage:
      case StateReplyMessage:
      case StateForwardMessage:
        DrawHelpText(composeMessageHelp);
        break;

      case StateViewPartList:
        DrawHelpText(viewPartListHelp);
        break;

      default:
        break;
    }

    wrefresh(m_HelpWin);
  }
}

void Ui::DrawHelpText(const std::vector<std::vector<std::string>>& p_HelpText)
{
  int cols = 6;
  int width = m_ScreenWidth / cols;

  int y = 0;
  for (auto rowIt = p_HelpText.begin(); rowIt != p_HelpText.end(); ++rowIt)
  {
    int x = 0;
    for (int colIdx = 0; colIdx < (int)rowIt->size(); colIdx += 2)
    {
      std::wstring wcmd = Util::ToWString(rowIt->at(colIdx));
      std::wstring wdesc = Util::ToWString(rowIt->at(colIdx + 1));

      wattron(m_HelpWin, A_REVERSE);
      mvwaddnwstr(m_HelpWin, y, x, wcmd.c_str(), wcmd.size());
      wattroff(m_HelpWin, A_REVERSE);

      const std::wstring wdescTrim = wdesc.substr(0, width - wcmd.size() - 2);
      mvwaddnwstr(m_HelpWin, y, x + wcmd.size() + 1, wdescTrim.c_str(), wdescTrim.size());

      x += width;
    }

    ++y;
  }
}

void Ui::DrawFolderList()
{
  if (!m_HasRequestedFolders)
  {
    ImapManager::Request request;
    request.m_GetFolders = true;
    LOG_DEBUG("async request folders");
    m_HasRequestedFolders = true;
    m_ImapManager->AsyncRequest(request);
  }

  werase(m_MainWin);

  std::set<std::string> folders;

  if (m_FolderListFilterStr.empty())
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    folders = m_Folders;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& folder : m_Folders)
    {
      if (Util::ToLower(folder).find(Util::ToLower(Util::ToString(m_FolderListFilterStr)))
          != std::string::npos)
      {
        folders.insert(folder);
      }
    }
  }

  int count = folders.size();
  if (count > 0)
  {
    if (m_FolderListCurrentIndex == INT_MAX)
    {
      for (int i = 0; i < count; ++i)
      {
        const std::string& folder = *std::next(folders.begin(), i);
        if (folder == m_CurrentFolder)
        {
          m_FolderListCurrentIndex = i;
        }
      }
    }

    m_FolderListCurrentIndex = Util::Bound(0, m_FolderListCurrentIndex, (int)folders.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = Util::Bound(0, (int)(m_FolderListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)folders.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)folders.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& folder = *std::next(folders.begin(), i);

      if (i == m_FolderListCurrentIndex)
      {
        wattron(m_MainWin, A_REVERSE);
        m_FolderListCurrentFolder = folder;
      }

      std::wstring wfolder = Util::ToWString(folder);
      mvwaddnwstr(m_MainWin, i - idxOffs, 2, wfolder.c_str(), wfolder.size());

      if (i == m_FolderListCurrentIndex)
      {
        wattroff(m_MainWin, A_REVERSE);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawAddressList()
{
  werase(m_MainWin);

  static std::wstring lastAddressListFilterStr = m_AddressListFilterStr;
  if (m_AddressListFilterStr != lastAddressListFilterStr)
  {
    m_Addresses = AddressBook::Get(Util::ToString(m_AddressListFilterStr));
    lastAddressListFilterStr = m_AddressListFilterStr;
  }

  int count = m_Addresses.size();
  if (count > 0)
  {
    m_AddressListCurrentIndex = Util::Bound(0, m_AddressListCurrentIndex, (int)m_Addresses.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = Util::Bound(0, (int)(m_AddressListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)m_Addresses.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)m_Addresses.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& address = *std::next(m_Addresses.begin(), i);

      if (i == m_AddressListCurrentIndex)
      {
        wattron(m_MainWin, A_REVERSE);
        m_AddressListCurrentAddress = address;
      }

      std::wstring waddress = Util::ToWString(address);
      size_t maxWidth = m_ScreenWidth - 4;
      if (waddress.size() > maxWidth)
      {
        static const std::wstring suffix = L"...";
        waddress = waddress.substr(0, maxWidth - suffix.size()) + suffix;
      }

      mvwaddnwstr(m_MainWin, i - idxOffs, 2, waddress.c_str(), waddress.size());

      if (i == m_AddressListCurrentIndex)
      {
        wattroff(m_MainWin, A_REVERSE);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawFileList()
{
  werase(m_MainWin);

  std::set<Fileinfo, FileinfoCompare> files;

  if (m_FileListFilterStr.empty())
  {
    files = m_Files;
  }
  else
  {
    for (const auto& file : m_Files)
    {
      if (Util::ToLower(file.m_Name).find(Util::ToLower(Util::ToString(m_FileListFilterStr)))
          != std::string::npos)
      {
        files.insert(file);
      }
    }
  }

  int count = files.size();
  if (count > 0)
  {
    m_FileListCurrentIndex = Util::Bound(0, m_FileListCurrentIndex, (int)files.size() - 1);

    int posOffs = 2;
    int itemsMax = m_MainWinHeight - 1 - posOffs;
    int idxOffs = Util::Bound(0, (int)(m_FileListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)files.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)files.size());

    int maxWidth = std::min(m_ScreenWidth - 4, 50);
    std::wstring dirLabel = L"Dir: ";
    int maxDirLen = maxWidth - dirLabel.size();
    std::wstring dirPath = Util::ToWString(m_CurrentDir);
    int dirPathRight = ((int)dirPath.size() < maxDirLen) ? 0 : (dirPath.size() - maxDirLen);
    dirLabel += dirPath.substr(dirPathRight);
    mvwaddnwstr(m_MainWin, 0, 2, dirLabel.c_str(), dirLabel.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const Fileinfo& fileinfo = *std::next(files.begin(), i);

      int maxNameLen = maxWidth - 2 - 7; // two spaces and (dir) / 1023 KB
      std::wstring name = Util::ToWString(fileinfo.m_Name);
      name = Util::TrimPadWString(name.substr(0, maxNameLen), maxNameLen);

      if (fileinfo.IsDir())
      {
        name += L"    (dir)";
      }
      else
      {
        // max 7 - ex: "1023 KB"
        std::string size = Util::GetPrefixedSize(fileinfo.m_Size);
        size = std::string(7 - size.size(), ' ') + size;
        name += L"  " + Util::ToWString(size);
      }

      if (i == m_FileListCurrentIndex)
      {
        wattron(m_MainWin, A_REVERSE);
        m_FileListCurrentFile = fileinfo;
      }

      mvwaddnwstr(m_MainWin, i - idxOffs + posOffs, 2, name.c_str(), name.size());

      if (i == m_FileListCurrentIndex)
      {
        wattroff(m_MainWin, A_REVERSE);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessageList()
{
  if (!m_HasRequestedUids[m_CurrentFolder])
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetUids = true;
    LOG_DEBUG_VAR("async request uids =", m_CurrentFolder);
    m_HasRequestedUids[m_CurrentFolder] = true;
    m_ImapManager->AsyncRequest(request);
  }

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t> fetchFlagUids;
  std::set<uint32_t> fetchBodyUids;
  std::set<uint32_t> prefetchBodyUids;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::set<uint32_t>& newUids = m_NewUids[m_CurrentFolder];
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, uint32_t>& flags = m_Flags[m_CurrentFolder];
    auto& msgDateUids = m_MsgDateUids[m_CurrentFolder];

    std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[m_CurrentFolder];
    std::set<uint32_t>& requestedFlags = m_RequestedFlags[m_CurrentFolder];

    if (!newUids.empty())
    {
      for (auto& uid : newUids)
      {
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

      newUids.clear();
    }

    const std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];
    std::set<uint32_t>& prefetchedBodys = m_PrefetchedBodys[m_CurrentFolder];
    std::set<uint32_t>& requestedBodys = m_RequestedBodys[m_CurrentFolder];

    int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] -
                                       ((m_MainWinHeight - 1) / 2)),
                              std::max(0, (int)msgDateUids.size() - (int)m_MainWinHeight));
    int idxMax = idxOffs + std::min(m_MainWinHeight, (int)msgDateUids.size());

    const std::string& currentDate = Header::GetCurrentDate();

    werase(m_MainWin);

    for (int i = idxOffs; i < idxMax; ++i)
    {
      uint32_t uid = std::prev(msgDateUids.end(), i + 1)->second;

      if ((flags.find(uid) == flags.end()) &&
          (requestedFlags.find(uid) == requestedFlags.end()))
      {
        fetchFlagUids.insert(uid);
        requestedFlags.insert(uid);
      }

      std::string seenFlag;
      if ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))))
      {
        seenFlag = std::string("N");
      }

      std::string shortDate;
      std::string shortFrom;
      std::string subject;
      std::string attachFlag;
      if (headers.find(uid) != headers.end())
      {
        Header& header = headers.at(uid);
        shortDate = header.GetDateOrTime(currentDate);
        subject = header.GetSubject();
        if (m_CurrentFolder == m_SentFolder)
        {
          shortFrom = header.GetShortTo();
        }
        else
        {
          shortFrom = header.GetShortFrom();
        }

        if (!m_AttachmentIndicator.empty())
        {
          static const std::wstring wIndicator = Util::ToWString(m_AttachmentIndicator);
          static const int indicatorWidth = wcswidth(wIndicator.c_str(), wIndicator.size());
          attachFlag = header.GetHasAttachments() ? std::string(m_AttachmentIndicator)
                                                  : std::string(indicatorWidth, ' ');
        }
      }

      seenFlag = Util::TrimPadString(seenFlag, 1);
      shortDate = Util::TrimPadString(shortDate, 10);
      shortFrom = Util::ToString(Util::TrimPadWString(Util::ToWString(shortFrom), 20));
      std::string headerLeft = " " + seenFlag + attachFlag + "  " + shortDate + "  " + shortFrom + "  ";
      int subjectWidth = m_ScreenWidth - Util::ToWString(headerLeft).size() - 1;
      subject = Util::ToString(Util::TrimPadWString(Util::ToWString(subject), subjectWidth));
      std::string header = headerLeft + subject + " ";

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        wattron(m_MainWin, A_REVERSE);
      }

      std::wstring wheader = Util::ToWString(header);
      mvwaddnwstr(m_MainWin, i - idxOffs, 0, wheader.c_str(), wheader.size());

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        wattroff(m_MainWin, A_REVERSE);
      }

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentMessage)
          {
            requestedBodys.insert(uid);
            fetchBodyUids.insert(uid);
          }
        }
      }
      else
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (prefetchedBodys.find(uid) == prefetchedBodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentView)
          {
            prefetchedBodys.insert(uid);
            prefetchBodyUids.insert(uid);
          }
        }
      }
    }
  }

  if (!fetchBodyUids.empty())
  {
    for (auto& uid : fetchBodyUids)
    {
      ImapManager::Request request;
      request.m_Folder = m_CurrentFolder;

      std::set<uint32_t> fetchUids;
      fetchUids.insert(uid);
      request.m_GetBodys = fetchUids;

      LOG_DEBUG_VAR("async request bodys =", fetchUids);
      m_ImapManager->AsyncRequest(request);
    }
  }

  if (!prefetchBodyUids.empty())
  {
    for (auto& uid : prefetchBodyUids)
    {
      ImapManager::Request request;
      request.m_PrefetchLevel = PrefetchLevelCurrentView;
      request.m_Folder = m_CurrentFolder;

      std::set<uint32_t> fetchUids;
      fetchUids.insert(uid);
      request.m_GetBodys = fetchUids;

      LOG_DEBUG_VAR("prefetch request bodys =", fetchUids);
      m_ImapManager->PrefetchRequest(request);
    }
  }

  const int maxHeadersFetchRequest = 25;
  if (!fetchHeaderUids.empty())
  {
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

        LOG_DEBUG_VAR("async request headers =", subsetFetchHeaderUids);
        m_ImapManager->AsyncRequest(request);

        subsetFetchHeaderUids.clear();
      }
    }
  }

  const int maxFlagsFetchRequest = 1000;
  if (!fetchFlagUids.empty())
  {
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

        LOG_DEBUG_VAR("async request flags =", subsetFetchFlagUids);
        m_ImapManager->AsyncRequest(request);

        subsetFetchFlagUids.clear();
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessageListSearch()
{
  const std::vector<Header>& headers = m_MessageListSearchResultHeaders;
  int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] - ((m_MainWinHeight - 1) / 2)),
                            std::max(0, (int)headers.size() - (int)m_MainWinHeight));
  int idxMax = idxOffs + std::min(m_MainWinHeight, (int)headers.size());
  const std::string& currentDate = Header::GetCurrentDate();
  std::map<std::string, std::set<uint32_t>> fetchFlagUids;
  std::map<std::string, std::set<uint32_t>> fetchBodyUids;

  werase(m_MainWin);
  for (int i = idxOffs; i < idxMax; ++i)
  {
    const std::string& folder = m_MessageListSearchResultFolderUids.at(i).first;
    const int uid = m_MessageListSearchResultFolderUids.at(i).second;

    std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
    std::set<uint32_t>& requestedFlags = m_RequestedFlags[folder];
    if ((flags.find(uid) == flags.end()) &&
        (requestedFlags.find(uid) == requestedFlags.end()))
    {
      fetchFlagUids[folder].insert(uid);
      requestedFlags.insert(uid);
    }

    std::string seenFlag;
    if ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))))
    {
      seenFlag = std::string("N");
    }

    std::string shortDate;
    std::string shortFrom;
    std::string subject;
    std::string attachFlag;
    {
      Header header = headers.at(i);
      shortDate = header.GetDateOrTime(currentDate);
      shortFrom = header.GetShortFrom();
      subject = header.GetSubject();

      if (!m_AttachmentIndicator.empty())
      {
        static const std::wstring wIndicator = Util::ToWString(m_AttachmentIndicator);
        static const int indicatorWidth = wcswidth(wIndicator.c_str(), wIndicator.size());
        attachFlag = header.GetHasAttachments() ? std::string(m_AttachmentIndicator)
                                                : std::string(indicatorWidth, ' ');
      }
    }

    seenFlag = Util::TrimPadString(seenFlag, 1);
    shortDate = Util::TrimPadString(shortDate, 10);
    shortFrom = Util::ToString(Util::TrimPadWString(Util::ToWString(shortFrom), 20));
    std::string headerLeft = " " + seenFlag + attachFlag + "  " + shortDate + "  " + shortFrom + "  ";
    int subjectWidth = m_ScreenWidth - Util::ToWString(headerLeft).size() - 1;
    subject = Util::ToString(Util::TrimPadWString(Util::ToWString(subject), subjectWidth));
    std::string header = headerLeft + subject + " ";

    if (i == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      wattron(m_MainWin, A_REVERSE);
    }

    std::wstring wheader = Util::ToWString(header);
    mvwaddnwstr(m_MainWin, i - idxOffs, 0, wheader.c_str(), wheader.size());

    if (i == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      wattroff(m_MainWin, A_REVERSE);
    }

    if (i == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      std::set<uint32_t>& requestedBodys = m_RequestedBodys[folder];

      if ((bodys.find(uid) == bodys.end()) &&
          (requestedBodys.find(uid) == requestedBodys.end()))
      {
        if (m_PrefetchLevel >= PrefetchLevelCurrentMessage)
        {
          requestedBodys.insert(uid);
          fetchBodyUids[folder].insert(uid);
        }
      }
    }
  }

  for (auto& fetchFlagUid : fetchFlagUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchFlagUid.first;
    request.m_GetFlags = fetchFlagUid.second;

    LOG_DEBUG_VAR("async request flags =", request.m_GetFlags);
    m_ImapManager->AsyncRequest(request);
  }

  for (auto& fetchBodyUid : fetchBodyUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchBodyUid.first;
    request.m_GetBodys = fetchBodyUid.second;

    LOG_DEBUG_VAR("async request bodys =", request.m_GetBodys);
    m_ImapManager->AsyncRequest(request);
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessage()
{
  werase(m_MainWin);

  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t> fetchBodyUids;
  bool markSeen = false;
  bool unseen = false;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[folder];

    if ((uid != -1) &&
        (headers.find(uid) == headers.end()) &&
        (requestedHeaders.find(uid) == requestedHeaders.end()))
    {
      requestedHeaders.insert(uid);
      fetchHeaderUids.insert(uid);
    }

    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::set<uint32_t>& requestedBodys = m_RequestedBodys[folder];

    if ((uid != -1) &&
        (bodys.find(uid) == bodys.end()) &&
        (requestedBodys.find(uid) == requestedBodys.end()))
    {
      requestedBodys.insert(uid);
      fetchBodyUids.insert(uid);
    }

    std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
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
      ss << "Date: " << header.GetDateTime() << "\n";
      ss << "From: " << header.GetFrom() << "\n";
      if (!header.GetReplyTo().empty())
      {
        ss << "Reply-To: " << header.GetReplyTo() << "\n";
      }

      ss << "To: " << header.GetTo() << "\n";
      if (!header.GetCc().empty())
      {
        ss << "Cc: " << header.GetCc() << "\n";
      }

      ss << "Subject: " << header.GetSubject() << "\n";

      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        std::map<ssize_t, Part> parts = body.GetParts();
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
          ss << "Attachments: ";
          ss << Util::Join(attnames, ", ");
          ss << "\n";
        }
      }

      ss << "\n";
    }

    headerText = ss.str();

    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& bodyText = m_Plaintext ? body.GetTextPlain() : body.GetTextHtml();
      const std::string text = headerText + bodyText;
      m_CurrentMessageViewText = text;
      const std::wstring wtext = Util::ToWString(text);
      const std::vector<std::wstring>& wlines = Util::WordWrap(wtext, m_MaxLineLength, true);
      int countLines = wlines.size();

      m_MessageViewLineOffset = Util::Bound(0, m_MessageViewLineOffset,
                                            countLines - m_MainWinHeight);
      for (int i = 0; ((i < m_MainWinHeight) && (i < countLines)); ++i)
      {
        const std::wstring& wdispStr = wlines.at(i + m_MessageViewLineOffset);
        const std::string& dispStr = Util::ToString(wdispStr);
        const bool isQuote = (dispStr.rfind(">", 0) == 0);

        if (m_ColorsEnabled && isQuote)
        {
          wattron(m_MainWin, COLOR_PAIR(m_ColorMessageQuoted));
        }

        mvwprintw(m_MainWin, i, 0, "%s", dispStr.c_str());

        if (m_ColorsEnabled && isQuote)
        {
          wattroff(m_MainWin, COLOR_PAIR(m_ColorMessageQuoted));
        }
      }

      markSeen = true;
    }
  }

  if (!fetchHeaderUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetHeaders = fetchHeaderUids;
    LOG_DEBUG_VAR("async request headers =", fetchHeaderUids);
    m_ImapManager->AsyncRequest(request);
  }

  if (!fetchBodyUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetBodys = fetchBodyUids;
    LOG_DEBUG_VAR("async request bodys =", fetchBodyUids);
    m_ImapManager->AsyncRequest(request);
  }

  if (unseen && markSeen && !m_MessageViewToggledSeen)
  {
    MarkSeen();
  }

  wrefresh(m_MainWin);
}

void Ui::DrawComposeMessage()
{
  m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_MaxLineLength, true,
                                         m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                         m_ComposeMessageWrapPos);

  std::vector<std::wstring> headerLines;
  if (m_ShowRichHeader)
  {
    headerLines =
    {
      L"To      : ",
      L"Cc      : ",
      L"Bcc     : ",
      L"Attchmnt: ",
      L"Subject : ",
    };
  }
  else
  {
    headerLines =
    {
      L"To      : ",
      L"Cc      : ",
      L"Attchmnt: ",
      L"Subject : ",
    };
  }

  int cursY = 0;
  int cursX = 0;
  if (m_IsComposeHeader)
  {
    if (m_ComposeHeaderLine < (int)m_ComposeHeaderStr.size())
    {
      cursY = m_ComposeHeaderLine;
      cursX = m_ComposeHeaderPos + 10;
    }
  }
  else
  {
    cursY = headerLines.size() + 1 + m_ComposeMessageWrapLine;
    cursX = m_ComposeMessageWrapPos;
  }

  werase(m_MainWin);

  std::vector<std::wstring> composeLines;

  for (int i = 0; i < (int)headerLines.size(); ++i)
  {
    if (m_IsComposeHeader && (i == m_ComposeHeaderLine) && (cursX >= m_ScreenWidth))
    {
      std::wstring line = headerLines.at(i) +
        m_ComposeHeaderStr.at(i).substr(cursX - m_ScreenWidth + 1);
      composeLines.push_back(line.substr(0, m_ScreenWidth));
      cursX = m_ScreenWidth - 1;
    }
    else
    {
      std::wstring line = headerLines.at(i) + m_ComposeHeaderStr.at(i);
      composeLines.push_back(line.substr(0, m_ScreenWidth));
    }
  }

  composeLines.push_back(L"");

  for (auto line = m_ComposeMessageLines.begin(); line != m_ComposeMessageLines.end(); ++line)
  {
    composeLines.push_back(*line);
  }

  if (cursY < m_ComposeMessageOffsetY)
  {
    m_ComposeMessageOffsetY = std::max(m_ComposeMessageOffsetY - (m_MainWinHeight / 2), 0);

  }
  else if (cursY >= (m_ComposeMessageOffsetY + m_MainWinHeight))
  {
    m_ComposeMessageOffsetY += (m_MainWinHeight / 2);
  }

  int messageY = 0;
  int idx = 0;
  for (auto line = composeLines.begin(); line != composeLines.end(); ++line, ++idx)
  {
    if (idx < m_ComposeMessageOffsetY) continue;

    if (messageY > m_MainWinHeight) break;

    const std::string& dispStr = Util::ToString(*line);
    mvwprintw(m_MainWin, messageY, 0, "%s", dispStr.c_str());
    ++messageY;
  }

  cursY -= m_ComposeMessageOffsetY;

  leaveok(m_MainWin, false);
  wmove(m_MainWin, cursY, cursX);
  wrefresh(m_MainWin);
  leaveok(m_MainWin, true);
}

void Ui::DrawPartList()
{
  werase(m_MainWin);

  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::map<uint32_t, Body>& bodys = m_Bodys[folder];
  std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
  if (bodyIt != bodys.end())
  {
    Body& body = bodyIt->second;
    const std::map<ssize_t, Part>& parts = body.GetParts();

    int count = parts.size();
    if (count > 0)
    {
      m_PartListCurrentIndex = Util::Bound(0, m_PartListCurrentIndex, (int)parts.size() - 1);

      int itemsMax = m_MainWinHeight - 1;
      int idxOffs = Util::Bound(0, (int)(m_PartListCurrentIndex - ((itemsMax - 1) / 2)),
                                std::max(0, (int)parts.size() - (int)itemsMax));
      int idxMax = idxOffs + std::min(itemsMax, (int)parts.size());

      for (int i = idxOffs; i < idxMax; ++i)
      {
        auto it = std::next(parts.begin(), i);
        const Part& part = it->second;

        if (i == m_PartListCurrentIndex)
        {
          wattron(m_MainWin, A_REVERSE);
          m_PartListCurrentPart = part;
        }

        std::string leftPad = "    ";
        std::string sizeStr = std::to_string(part.m_Data.size()) + " bytes";
        std::string sizeStrPadded = Util::TrimPadString(sizeStr, 18);
        std::string mimeTypePadded = Util::TrimPadString(part.m_MimeType, 30);
        std::string line = leftPad + sizeStrPadded + mimeTypePadded;
        std::string filenamePadded =
          Util::TrimPadString(part.m_Filename, std::max(m_ScreenWidth - (int)line.size(), 0));
        line = line + filenamePadded;

        std::wstring wline = Util::ToWString(line);
        mvwaddnwstr(m_MainWin, i - idxOffs, 0, wline.c_str(),
                    std::min((int)wline.size(), m_ScreenWidth));

        if (i == m_PartListCurrentIndex)
        {
          wattroff(m_MainWin, A_REVERSE);
        }
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::AsyncUiRequest(char p_UiRequest)
{
  write(m_Pipe[1], &p_UiRequest, 1);
}

void Ui::PerformUiRequest(char p_UiRequest)
{
  if (p_UiRequest & UiRequestDrawAll)
  {
    DrawAll();
  }

  if (p_UiRequest & UiRequestDrawError)
  {
    std::unique_lock<std::mutex> lock(m_SmtpErrorMutex);
    if (!m_SmtpErrorResults.empty())
    {
      const SmtpManager::Result result = m_SmtpErrorResults.front();
      m_SmtpErrorResults.pop_front();
      lock.unlock();

      SmtpResultHandlerError(result);
    }
  }
}

void Ui::Run()
{
  DrawAll();
  int64_t uiIdleTime = 0;
  LOG_DEBUG("entering loop");

  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = std::max(STDIN_FILENO, m_Pipe[0]);
    struct timeval tv = {1, 0}; // uiIdleTime logic below is dependent on timeout value
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0)
    {
      if (++uiIdleTime >= 600) // ui idle refresh every 10 minutes
      {
        PerformUiRequest(UiRequestDrawAll);
        uiIdleTime = 0;
      }

      continue;
    }

    uiIdleTime = 0;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      if (key == KEY_RESIZE)
      {
        CleanupWindows();
        InitWindows();
        DrawAll();
        continue;
      }

      switch (m_State)
      {
        case StateViewMessageList:
          ViewMessageListKeyHandler(key);
          break;

        case StateViewMessage:
          ViewMessageKeyHandler(key);
          break;

        case StateGotoFolder:
        case StateMoveToFolder:
          ViewFolderListKeyHandler(key);
          break;

        case StateComposeMessage:
        case StateReplyMessage:
        case StateForwardMessage:
          ComposeMessageKeyHandler(key);
          break;

        case StateAddressList:
          ViewAddressListKeyHandler(key);
          break;

        case StateFileList:
          ViewFileListKeyHandler(key);
          break;

        case StateViewPartList:
          ViewPartListKeyHandler(key);
          break;

        default:
          break;
      }

      continue;
    }

    if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        read(m_Pipe[0], &buf[0], len);
        char uiRequest = UiRequestNone;
        for (int i = 0; i < len; ++i)
        {
          uiRequest |= buf[i];
        }

        PerformUiRequest(uiRequest);
      }
    }

  }

  LOG_DEBUG("exiting loop");

  return;
}

void Ui::ViewFolderListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(StateViewMessageList);
  }
  else if (p_Key == KEY_UP)
  {
    --m_FolderListCurrentIndex;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_FolderListCurrentIndex;
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_FolderListCurrentIndex = m_FolderListCurrentIndex - m_MainWinHeight;
  }
  else if (p_Key == m_KeyNextPage)
  {
    m_FolderListCurrentIndex = m_FolderListCurrentIndex + m_MainWinHeight;
  }
  else if (p_Key == KEY_HOME)
  {
    m_FolderListCurrentIndex = 0;
  }
  else if (p_Key == KEY_END)
  {
    m_FolderListCurrentIndex = std::numeric_limits<int>::max();
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) ||
           ((p_Key == KEY_RIGHT) && (m_FolderListFilterPos == (int)m_FolderListFilterStr.size())))
  {
    if (m_State == StateGotoFolder)
    {
      m_CurrentFolder = m_FolderListCurrentFolder;
      m_ImapManager->SetCurrentFolder(m_CurrentFolder);
      SetState(StateViewMessageList);
      UpdateIndexFromUid();
    }
    else if (m_State == StateMoveToFolder)
    {
      const std::string& folder = m_CurrentFolderUid.first;
      if (m_FolderListCurrentFolder != folder)
      {
        const int uid = m_CurrentFolderUid.second;
        MoveMessage(uid, folder, m_FolderListCurrentFolder);
        UpdateUidFromIndex(true /* p_UserTriggered */);
        SetLastStateOrMessageList();
      }
      else
      {
        SetDialogMessage("Move to same folder ignored");
        UpdateUidFromIndex(true /* p_UserTriggered */);
        SetState(m_LastState);
      }
    }
  }
  else if (p_Key == KEY_LEFT)
  {
    m_FolderListFilterPos = Util::Bound(0, m_FolderListFilterPos - 1,
                                        (int)m_FolderListFilterStr.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    m_FolderListFilterPos = Util::Bound(0, m_FolderListFilterPos + 1,
                                        (int)m_FolderListFilterStr.size());
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE))
  {
    if (m_FolderListFilterPos > 0)
    {
      m_FolderListFilterStr.erase(--m_FolderListFilterPos, 1);
    }
  }
  else if (p_Key == KEY_DC)
  {
    if (m_FolderListFilterPos < (int)m_FolderListFilterStr.size())
    {
      m_FolderListFilterStr.erase(m_FolderListFilterPos, 1);
    }
  }
  else if (IsValidTextKey(p_Key))
  {
    m_FolderListFilterStr.insert(m_FolderListFilterPos++, 1, p_Key);
  }

  DrawAll();
}

void Ui::ViewAddressListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if (p_Key == KEY_UP)
  {
    --m_AddressListCurrentIndex;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_AddressListCurrentIndex;
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_AddressListCurrentIndex = m_AddressListCurrentIndex - m_MainWinHeight;
  }
  else if (p_Key == m_KeyNextPage)
  {
    m_AddressListCurrentIndex = m_AddressListCurrentIndex + m_MainWinHeight;
  }
  else if (p_Key == KEY_HOME)
  {
    m_AddressListCurrentIndex = 0;
  }
  else if (p_Key == KEY_END)
  {
    m_AddressListCurrentIndex = std::numeric_limits<int>::max();
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) ||
           ((p_Key == KEY_RIGHT) && (m_AddressListFilterPos == (int)m_AddressListFilterStr.size())))
  {
    std::wstring address;
    const std::string& oldAddress =
      Util::Trim(Util::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine].substr(0, m_ComposeHeaderPos)));
    if (!oldAddress.empty() && (oldAddress[oldAddress.size() - 1] != ','))
    {
      address = Util::ToWString(", " + m_AddressListCurrentAddress);
    }
    else
    {
      address = Util::ToWString(m_AddressListCurrentAddress);
    }

    m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos, address);
    m_ComposeHeaderPos += address.size();
    SetState(m_LastMessageState);
  }
  else if (p_Key == KEY_LEFT)
  {
    m_AddressListFilterPos = Util::Bound(0, m_AddressListFilterPos - 1,
                                         (int)m_AddressListFilterStr.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    m_AddressListFilterPos = Util::Bound(0, m_AddressListFilterPos + 1,
                                         (int)m_AddressListFilterStr.size());
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE))
  {
    if (m_AddressListFilterPos > 0)
    {
      m_AddressListFilterStr.erase(--m_AddressListFilterPos, 1);
    }
  }
  else if (p_Key == KEY_DC)
  {
    if (m_AddressListFilterPos < (int)m_AddressListFilterStr.size())
    {
      m_AddressListFilterStr.erase(m_AddressListFilterPos, 1);
    }
  }
  else if (IsValidTextKey(p_Key))
  {
    m_AddressListFilterStr.insert(m_AddressListFilterPos++, 1, p_Key);
  }

  DrawAll();
}

void Ui::ViewFileListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if (p_Key == KEY_UP)
  {
    --m_FileListCurrentIndex;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_FileListCurrentIndex;
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_FileListCurrentIndex = m_FileListCurrentIndex - m_MainWinHeight;
  }
  else if (p_Key == m_KeyNextPage)
  {
    m_FileListCurrentIndex = m_FileListCurrentIndex + m_MainWinHeight;
  }
  else if (p_Key == KEY_HOME)
  {
    m_FileListCurrentIndex = 0;
  }
  else if (p_Key == KEY_END)
  {
    m_FileListCurrentIndex = std::numeric_limits<int>::max();
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) ||
           ((p_Key == KEY_RIGHT) && (m_FileListFilterPos == (int)m_FileListFilterStr.size())))
  {
    if (m_FileListCurrentFile.IsDir())
    {
      m_FileListFilterPos = 0;
      m_FileListFilterStr.clear();
      m_CurrentDir = Util::AbsolutePath(m_CurrentDir + "/" + m_FileListCurrentFile.m_Name);
      m_Files = Util::ListPaths(m_CurrentDir);
      m_FileListCurrentIndex = 0;
      m_FileListCurrentFile.m_Name = "";
    }
    else
    {
      std::string newFilePath = Util::AbsolutePath(m_CurrentDir + "/" + m_FileListCurrentFile.m_Name);
      std::wstring filepaths;
      const std::string& oldFilepaths =
        Util::Trim(Util::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine].substr(0, m_ComposeHeaderPos)));
      if (!oldFilepaths.empty() && (oldFilepaths[oldFilepaths.size() - 1] != ','))
      {
        filepaths = Util::ToWString(", " + newFilePath);
      }
      else
      {
        filepaths = Util::ToWString(newFilePath);
      }

      m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos, filepaths);
      m_ComposeHeaderPos += filepaths.size();
      SetState(m_LastMessageState);
    }
  }
  else if ((p_Key == KEY_LEFT) && (m_FileListFilterPos == 0))
  {
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    m_CurrentDir = Util::AbsolutePath(m_CurrentDir + "/..");
    m_Files = Util::ListPaths(m_CurrentDir);
    m_FileListCurrentIndex = 0;
    m_FileListCurrentFile.m_Name = "";
  }
  else if (p_Key == KEY_LEFT)
  {
    m_FileListFilterPos = Util::Bound(0, m_FileListFilterPos - 1,
                                      (int)m_FileListFilterStr.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    m_FileListFilterPos = Util::Bound(0, m_FileListFilterPos + 1,
                                      (int)m_FileListFilterStr.size());
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE))
  {
    if (m_FileListFilterPos > 0)
    {
      m_FileListFilterStr.erase(--m_FileListFilterPos, 1);
    }
    else
    {
      // backspace with empty filter goes up one directory level
      m_FileListFilterPos = 0;
      m_FileListFilterStr.clear();
      m_CurrentDir = Util::AbsolutePath(m_CurrentDir + "/..");
      m_Files = Util::ListPaths(m_CurrentDir);
      m_FileListCurrentIndex = 0;
      m_FileListCurrentFile.m_Name = "";
    }
  }
  else if (p_Key == KEY_DC)
  {
    if (m_FileListFilterPos < (int)m_FileListFilterStr.size())
    {
      m_FileListFilterStr.erase(m_FileListFilterPos, 1);
    }
  }
  else if (IsValidTextKey(p_Key))
  {
    m_FileListFilterStr.insert(m_FileListFilterPos++, 1, p_Key);
  }

  DrawAll();
}

void Ui::ViewMessageListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if (p_Key == m_KeyRefresh)
  {
    if (IsConnected())
    {
      InvalidateUiCache(m_CurrentFolder);
    }
    else
    {
      SetDialogMessage("Cannot refresh while offline");
    }
  }
  else if ((p_Key == KEY_UP) || (p_Key == m_KeyPrevMsg))
  {
    --m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_DOWN) || (p_Key == m_KeyNextMsg))
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
  else if ((p_Key == m_KeyNextPage) || (p_Key == KEY_SPACE))
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == KEY_HOME)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == KEY_END)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = std::numeric_limits<int>::max();
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) || (p_Key == m_KeyOpen) || (p_Key == KEY_RIGHT))
  {
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      SetState(StateViewMessage);
    }
  }
  else if ((p_Key == m_KeyGotoFolder) || (p_Key == m_KeyBack) || (p_Key == KEY_LEFT))
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_CurrentFolder = m_PreviousFolder;
      m_PreviousFolder = "";
      UpdateIndexFromUid();
    }
    else
    {
      SetState(StateGotoFolder);
    }
  }
  else if (p_Key == m_KeyMove)
  {
    if (IsConnected())
    {
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        SetState(StateMoveToFolder);
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
    if (IsConnected())
    {
      SetState(StateComposeMessage);
    }
    else
    {
      SetDialogMessage("Cannot compose while offline");
    }
  }
  else if (p_Key == m_KeyReply)
  {
    if (IsConnected())
    {
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        if (CurrentMessageBodyAvailable())
        {
          SetState(StateReplyMessage);
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
    else
    {
      SetDialogMessage("Cannot reply while offline");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    if (IsConnected())
    {
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        if (CurrentMessageBodyAvailable())
        {
          SetState(StateForwardMessage);
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
    else
    {
      SetDialogMessage("Cannot forward while offline");
    }
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == KEY_DC))
  {
    if (IsConnected())
    {
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
    m_HelpViewMessagesListOffset = (m_HelpViewMessagesListOffset == 0) ? 2 : 0;
  }
  else if (p_Key == m_KeyExport)
  {
    ExportMessage();
  }
  else if (p_Key == m_KeyImport)
  {
    ImportMessage();
  }
  else if (p_Key == m_KeySearch)
  {
    SearchMessage();
  }
  else if (p_Key == m_KeySync)
  {
    StartSync();
  }
  else if (p_Key == m_KeyExtHtmlViewer)
  {
    ExtHtmlViewer();
  }
  else if (p_Key == m_KeyExtMsgViewer)
  {
    ExtMsgViewer();
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void Ui::ViewMessageKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if (p_Key == KEY_UP)
  {
    --m_MessageViewLineOffset;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_MessageViewLineOffset;
  }
  else if (p_Key == m_KeyPrevMsg)
  {
    --m_MessageListCurrentIndex[m_CurrentFolder];
    m_MessageViewLineOffset = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyNextMsg)
  {
    ++m_MessageListCurrentIndex[m_CurrentFolder];
    m_MessageViewLineOffset = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset - m_MainWinHeight;
  }
  else if ((p_Key == m_KeyNextPage) || (p_Key == KEY_SPACE))
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset + m_MainWinHeight;
  }
  else if (p_Key == KEY_HOME)
  {
    m_MessageViewLineOffset = 0;
  }
  else if (p_Key == KEY_END)
  {
    m_MessageViewLineOffset = std::numeric_limits<int>::max();
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE) || (p_Key == m_KeyBack) || (p_Key == KEY_LEFT))
  {
    SetState(StateViewMessageList);
  }
  else if ((p_Key == m_KeyOpen) || (p_Key == KEY_RIGHT))
  {
    SetState(StateViewPartList);
  }
  else if (p_Key == m_KeyGotoFolder)
  {
    SetState(StateGotoFolder);
  }
  else if (p_Key == m_KeyMove)
  {
    if (IsConnected())
    {
      SetState(StateMoveToFolder);
    }
    else
    {
      SetDialogMessage("Cannot move while offline");
    }
  }
  else if (p_Key == m_KeyCompose)
  {
    if (IsConnected())
    {
      SetState(StateComposeMessage);
    }
    else
    {
      SetDialogMessage("Cannot compose while offline");
    }
  }
  else if (p_Key == m_KeyReply)
  {
    if (IsConnected())
    {
      if (CurrentMessageBodyAvailable())
      {
        SetState(StateReplyMessage);
      }
      else
      {
        SetDialogMessage("Cannot reply message not fetched");
      }
    }
    else
    {
      SetDialogMessage("Cannot reply while offline");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    if (IsConnected())
    {
      if (CurrentMessageBodyAvailable())
      {
        SetState(StateForwardMessage);
      }
      else
      {
        SetDialogMessage("Cannot forward message not fetched");
      }
    }
    else
    {
      SetDialogMessage("Cannot forward while offline");
    }
  }
  else if (p_Key == m_KeyToggleTextHtml)
  {
    m_Plaintext = !m_Plaintext;
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == KEY_DC))
  {
    if (IsConnected())
    {
      DeleteMessage();
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_MessageViewToggledSeen = true;
      }
      ToggleSeen();
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else if (p_Key == m_KeyOtherCmdHelp)
  {
    m_HelpViewMessageOffset = (m_HelpViewMessageOffset == 0) ? 2 : 0;
  }
  else if (p_Key == m_KeyExport)
  {
    ExportMessage();
  }
  else if (p_Key == m_KeyExtPager)
  {
    ExtPager();
  }
  else if (p_Key == m_KeyExtHtmlViewer)
  {
    ExtHtmlViewer();
  }
  else if (p_Key == m_KeyExtMsgViewer)
  {
    ExtMsgViewer();
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void Ui::ComposeMessageKeyHandler(int p_Key)
{
  bool continueProcess = false;
  bool asyncRedraw = false;

  // process state-specific key handling first
  if (m_IsComposeHeader)
  {
    if (p_Key == KEY_UP)
    {
      --m_ComposeHeaderLine;
      if (m_ComposeHeaderLine < 0)
      {
        m_ComposeHeaderPos = 0;
      }

      m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine,
                                        (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == KEY_DOWN) || (p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) || (p_Key == KEY_TAB))
    {
      if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
      {
        m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine + 1,
                                          (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_IsComposeHeader = false;
      }
    }
    else if (p_Key == m_KeyPrevPage)
    {
      m_ComposeHeaderLine = 0;
      m_ComposeHeaderPos = 0;
    }
    else if (p_Key == m_KeyNextPage)
    {
      m_IsComposeHeader = false;
    }
    else if (p_Key == KEY_HOME)
    {
      // @todo: implement home/end handling in compose
    }
    else if (p_Key == KEY_END)
    {
      // @todo: implement home/end handling in compose
    }
    else if (p_Key == KEY_LEFT)
    {
      --m_ComposeHeaderPos;
      if (m_ComposeHeaderPos < 0)
      {
        --m_ComposeHeaderLine;
        if (m_ComposeHeaderLine < 0)
        {
          m_ComposeHeaderPos = 0;
        }
        else
        {
          m_ComposeHeaderPos = std::numeric_limits<int>::max();
        }

        m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine,
                                          (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
    }
    else if (p_Key == KEY_RIGHT)
    {
      ++m_ComposeHeaderPos;
      if (m_ComposeHeaderPos > (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size())
      {
        m_ComposeHeaderPos = 0;

        if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
        {
          m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine + 1,
                                            (int)m_ComposeHeaderStr.size() - 1);
          m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                           (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
        }
        else
        {
          m_IsComposeHeader = false;
          m_ComposeMessagePos = 0;
        }
      }
      else
      {
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
    }
    else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE))
    {
      if (m_ComposeHeaderPos > 0)
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].erase(--m_ComposeHeaderPos, 1);
      }
    }
    else if (p_Key == KEY_DC)
    {
      if (m_ComposeHeaderPos < (int)m_ComposeHeaderStr[m_ComposeHeaderLine].size())
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].erase(m_ComposeHeaderPos, 1);
      }
    }
    else if (p_Key == m_KeyDeleteLine)
    {
      Util::DeleteToMatch(m_ComposeHeaderStr[m_ComposeHeaderLine], m_ComposeHeaderPos, '\n');
    }
    else if (p_Key == m_KeyToSelect)
    {
      int headerField = GetCurrentHeaderField();
      if ((headerField == HeaderTo) || (headerField == HeaderCc) || (headerField == HeaderBcc))
      {
        SetState(StateAddressList);
      }
      else if (headerField == HeaderAtt)
      {
        SetState(StateFileList);
      }
    }
    else
    {
      continueProcess = true;
    }
  }
  else // compose body
  {
    if (p_Key == KEY_UP)
    {
      ComposeMessagePrevLine();
    }
    else if (p_Key == KEY_DOWN)
    {
      ComposeMessageNextLine();
    }
    else if (p_Key == m_KeyPrevPage)
    {
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessagePrevLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_MaxLineLength, true,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == m_KeyNextPage)
    {
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessageNextLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_MaxLineLength, true,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == KEY_HOME)
    {
      // @todo: implement home/end handling in compose
    }
    else if (p_Key == KEY_END)
    {
      // @todo: implement home/end handling in compose
    }
    else if (p_Key == KEY_LEFT)
    {
      --m_ComposeMessagePos;
      if (m_ComposeMessagePos < 0)
      {
        m_IsComposeHeader = true;
        m_ComposeHeaderPos = (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size();
      }

      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos,
                                        (int)m_ComposeMessageStr.size());
    }
    else if (p_Key == KEY_RIGHT)
    {
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + 1,
                                        (int)m_ComposeMessageStr.size());
    }
    else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE))
    {
      if (m_ComposeMessagePos > 0)
      {
        m_ComposeMessageStr.erase(--m_ComposeMessagePos, 1);
      }
    }
    else if (p_Key == KEY_DC)
    {
      if (m_ComposeMessagePos < (int)m_ComposeMessageStr.size())
      {
        m_ComposeMessageStr.erase(m_ComposeMessagePos, 1);
      }
    }
    else if (p_Key == m_KeyDeleteLine)
    {
      Util::DeleteToMatch(m_ComposeMessageStr, m_ComposeMessagePos, '\n');
    }
    else if (p_Key == m_KeyBackwardWord)
    {
      size_t searchPos = (m_ComposeMessagePos > 0) ? (m_ComposeMessagePos - 2) : 0;
      size_t prevSpacePos = m_ComposeMessageStr.rfind(' ', searchPos);
      if (prevSpacePos != std::string::npos)
      {
        m_ComposeMessagePos = Util::Bound(0, (int)prevSpacePos + 1, (int)m_ComposeMessageStr.size());
      }
      else
      {
        m_ComposeMessagePos = 0;
      }
    }
    else if (p_Key == m_KeyForwardWord)
    {
      size_t searchPos = m_ComposeMessagePos + 1;
      size_t nextSpacePos = m_ComposeMessageStr.find(' ', searchPos);
      if (nextSpacePos != std::string::npos)
      {
        m_ComposeMessagePos = Util::Bound(0, (int)nextSpacePos, (int)m_ComposeMessageStr.size());
      }
      else
      {
        m_ComposeMessagePos = m_ComposeMessageStr.size();
      }
    }
    else if (p_Key == m_KeyBackwardKillWord)
    {
      Util::DeleteToPrevMatch(m_ComposeMessageStr, m_ComposeMessagePos, ' ');
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos, (int)m_ComposeMessageStr.size());
    }
    else if (p_Key == m_KeyKillWord)
    {
      Util::DeleteToNextMatch(m_ComposeMessageStr, m_ComposeMessagePos, ' ');
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos, (int)m_ComposeMessageStr.size());
    }
    else
    {
      continueProcess = true;
    }
  }

  // process common key handling last (if not handled above)
  if (continueProcess)
  {
    if (p_Key == m_KeyCancel)
    {
      if (m_CancelWithoutConfirm || Ui::PromptYesNo("Cancel message (y/n)?"))
      {
        Util::RmDir(Util::GetPreviewTempDir());
        UpdateUidFromIndex(true /* p_UserTriggered */);
        SetLastStateOrMessageList();
        Util::RmDir(m_ComposeTempDirectory);
      }
    }
    else if (p_Key == m_KeySend)
    {
      if (m_SendWithoutConfirm || Ui::PromptYesNo("Send message (y/n)?"))
      {
        Util::RmDir(Util::GetPreviewTempDir());
        SendComposedMessage();
        UpdateUidFromIndex(true /* p_UserTriggered */);
        if (m_ComposeDraftUid != 0)
        {
          SetState(StateViewMessageList);
        }
        else
        {
          SetLastStateOrMessageList();
        }
      }
    }
    else if (p_Key == m_KeyPostpone)
    {
      if (m_PostponeWithoutConfirm || Ui::PromptYesNo("Postpone message (y/n)?"))
      {
        Util::RmDir(Util::GetPreviewTempDir());
        UploadDraftMessage();
        UpdateUidFromIndex(true /* p_UserTriggered */);
        if (m_ComposeDraftUid != 0)
        {
          SetState(StateViewMessageList);
        }
        else
        {
          SetLastStateOrMessageList();
        }
      }
    }
    else if (p_Key == m_KeyExtEditor)
    {
      ExtEditor(m_ComposeMessageStr, m_ComposeMessagePos);
    }
    else if (p_Key == m_KeyRichHeader)
    {
      std::wstring to = GetComposeStr(HeaderTo);
      std::wstring cc = GetComposeStr(HeaderCc);
      std::wstring bcc = GetComposeStr(HeaderBcc);
      std::wstring att = GetComposeStr(HeaderAtt);
      std::wstring sub = GetComposeStr(HeaderSub);

      m_ShowRichHeader = !m_ShowRichHeader;

      SetComposeStr(HeaderAll, L"");
      SetComposeStr(HeaderTo, to);
      SetComposeStr(HeaderCc, cc);
      SetComposeStr(HeaderBcc, bcc);
      SetComposeStr(HeaderAtt, att);
      SetComposeStr(HeaderSub, sub);
    }
    else if (p_Key == m_KeyExtHtmlPreview)
    {
      if (m_CurrentMarkdownHtmlCompose)
      {
        std::string tempFilePath = Util::GetPreviewTempDir() + "msg.html";
        std::string htmlStr = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
        Util::WriteFile(tempFilePath, htmlStr);
        ExtHtmlViewer(tempFilePath);
      }
      else
      {
        SetDialogMessage("Markdown compose is not enabled");
      }
    }
    else if (p_Key == m_KeyToggleMarkdownCompose)
    {
      m_CurrentMarkdownHtmlCompose = !m_CurrentMarkdownHtmlCompose;
    }
    else if (IsValidTextKey(p_Key))
    {
      if (m_IsComposeHeader)
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos++, 1, p_Key);
      }
      else
      {
        m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, p_Key);
      }

      asyncRedraw = true;
    }
    else
    {
      SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
    }
  }

  if (asyncRedraw)
  {
    AsyncUiRequest(UiRequestDrawAll);
  }
  else
  {
    DrawAll();
  }
}

void Ui::ViewPartListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if ((p_Key == KEY_UP) || (p_Key == m_KeyPrevMsg))
  {
    --m_PartListCurrentIndex;
  }
  else if ((p_Key == KEY_DOWN) || (p_Key == m_KeyNextMsg))
  {
    ++m_PartListCurrentIndex;
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_PartListCurrentIndex = m_PartListCurrentIndex - m_MainWinHeight;
  }
  else if ((p_Key == m_KeyNextPage) || (p_Key == KEY_SPACE))
  {
    m_PartListCurrentIndex = m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight;
  }
  else if (p_Key == KEY_HOME)
  {
    m_PartListCurrentIndex = 0;
  }
  else if (p_Key == KEY_END)
  {
    m_PartListCurrentIndex = std::numeric_limits<int>::max();
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE) || (p_Key == m_KeyBack) || (p_Key == KEY_LEFT))
  {
    const std::string& attachmentsTempDir = Util::GetAttachmentsTempDir();
    LOG_DEBUG("deleting %s", attachmentsTempDir.c_str());
    Util::CleanupAttachmentsTempDir();
    SetState(StateViewMessage);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) || (p_Key == m_KeyOpen) || (p_Key == KEY_RIGHT))
  {
    std::string ext;
    std::string err;
    std::string fileName;
    bool isUnamedTextHtml = false;
    if (!m_PartListCurrentPart.m_Filename.empty())
    {
      ext = Util::GetFileExt(m_PartListCurrentPart.m_Filename);
      fileName = m_PartListCurrentPart.m_Filename;
      if (ext.empty())
      {
        LOG_DEBUG("cannot determine file extension for %s", m_PartListCurrentPart.m_Filename.c_str());
      }
    }
    else
    {
      ext = Util::ExtensionForMimeType(m_PartListCurrentPart.m_MimeType);
      fileName = std::to_string(m_PartListCurrentIndex) + ext;
      isUnamedTextHtml = (m_PartListCurrentPart.m_MimeType == "text/html");
      if (ext.empty())
      {
        LOG_DEBUG("no file extension for MIME type %s", m_PartListCurrentPart.m_MimeType.c_str());
      }
    }

    std::string tempFilePath;

    if (m_ShowEmbeddedImages && isUnamedTextHtml)
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      const std::string& folder = m_CurrentFolderUid.first;
      const int uid = m_CurrentFolderUid.second;
      std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        const std::map<ssize_t, Part>& parts = body.GetParts();
        for (auto& part : parts)
        {
          if (!part.second.m_ContentId.empty())
          {
            const std::string& tempPartFilePath = Util::GetAttachmentsTempDir() + part.second.m_ContentId;
            LOG_DEBUG("writing \"%s\"", tempPartFilePath.c_str());
            Util::WriteFile(tempPartFilePath, part.second.m_Data);
          }
        }
      }

      tempFilePath = Util::GetAttachmentsTempDir() + fileName;
      std::string partData = m_PartListCurrentPart.m_Data;
      Util::ReplaceString(partData, "src=cid:", "src=file://" + Util::GetAttachmentsTempDir());
      Util::ReplaceString(partData, "src=\"cid:", "src=\"file://" + Util::GetAttachmentsTempDir());
      LOG_DEBUG("writing \"%s\"", tempFilePath.c_str());
      Util::WriteFile(tempFilePath, partData);
    }
    else
    {
      tempFilePath = Util::GetAttachmentsTempDir() + fileName;
      LOG_DEBUG("writing \"%s\"", tempFilePath.c_str());
      Util::WriteFile(tempFilePath, m_PartListCurrentPart.m_Data);
    }

    LOG_DEBUG("opening \"%s\" in external viewer", tempFilePath.c_str());

    SetDialogMessage("Waiting for external viewer to exit");
    DrawDialog();
    int rv = ExtPartsViewer(tempFilePath);
    if (rv != 0)
    {
      SetDialogMessage("External viewer error code " + std::to_string(rv), true /* p_Warn */);
    }
    else
    {
      LOG_DEBUG("external viewer exited successfully");
      SetDialogMessage("");
    }
  }
  else if (p_Key == m_KeySaveFile)
  {
    std::string filename = m_PartListCurrentPart.m_Filename;
    if (PromptString("Save Filename: ", "Save", filename))
    {
      if (!filename.empty())
      {
        filename = Util::ExpandPath(filename);
        Util::WriteFile(filename, m_PartListCurrentPart.m_Data);
        SetDialogMessage("File saved");
      }
      else
      {
        SetDialogMessage("Save cancelled (empty filename)");
      }
    }
    else
    {
      SetDialogMessage("Save cancelled");
    }
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void Ui::SetState(Ui::State p_State)
{
  if ((p_State == StateAddressList) || (p_State == StateFileList))
  {
    // going to address or file list
    m_LastMessageState = m_State;
    m_State = p_State;
  }
  else if ((m_State != StateAddressList) && (m_State != StateFileList))
  {
    // normal state transition
    m_LastState = m_State;
    m_State = p_State;
  }
  else
  {
    // exiting address or file list
    m_State = p_State;
    return;
  }

  if (m_State == StateGotoFolder)
  {
    curs_set(1);
    m_FolderListFilterPos = 0;
    m_FolderListFilterStr.clear();
    m_FolderListCurrentFolder = m_CurrentFolder;
    m_FolderListCurrentIndex = INT_MAX;
  }
  else if (m_State == StateMoveToFolder)
  {
    curs_set(1);
    if (!m_PersistFolderFilter)
    {
      m_FolderListFilterPos = 0;
      m_FolderListFilterStr.clear();
      m_FolderListCurrentFolder = m_CurrentFolder;
      m_FolderListCurrentIndex = INT_MAX;
    }
  }
  else if (m_State == StateViewMessageList)
  {
    curs_set(0);
    m_HelpViewMessagesListOffset = 0;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_MessageViewToggledSeen = false;
    }
  }
  else if (m_State == StateViewMessage)
  {
    curs_set(0);
    m_MessageViewLineOffset = 0;
    m_HelpViewMessageOffset = 0;
  }
  else if (m_State == StateComposeMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    m_ComposeHeaderLine = 0;
    m_ComposeHeaderPos = 0;
    m_ComposeHeaderRef.clear();
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_IsComposeHeader = true;
    m_ComposeDraftUid = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    if (folder == m_DraftsFolder)
    {
      std::map<uint32_t, Header>& headers = m_Headers[folder];
      std::map<uint32_t, Body>& bodys = m_Bodys[folder];

      std::map<uint32_t, Header>::iterator hit = headers.find(uid);
      std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
      if ((hit != headers.end()) && (bit != bodys.end()))
      {
        m_ComposeDraftUid = uid;

        Header& header = hit->second;
        Body& body = bit->second;

        const std::string& bodyText = body.GetTextPlain();
        m_ComposeMessageStr = Util::ToWString(bodyText);
        Util::StripCR(m_ComposeMessageStr);

        // @todo: handle quoted commas in address name
        std::vector<std::string> tos;
        std::vector<std::string> ccs;
        std::vector<std::string> bccs;
        if (header.GetReplyTo().empty())
        {
          tos = Util::Split(header.GetTo(), ',');
          ccs = Util::Split(header.GetCc(), ',');
          bccs = Util::Split(header.GetBcc(), ',');
          if (!bccs.empty())
          {
            // @todo: consider auto-revert to previous rich header state after send / postpone
            m_ShowRichHeader = true;
          }
        }
        else
        {
          tos = Util::Split(header.GetReplyTo(), ',');
        }

        SetComposeStr(HeaderTo, Util::ToWString(Util::Join(tos, ", ")));
        SetComposeStr(HeaderCc, Util::ToWString(Util::Join(ccs, ", ")));
        SetComposeStr(HeaderBcc, Util::ToWString(Util::Join(bccs, ", ")));
        SetComposeStr(HeaderAtt, L"");
        SetComposeStr(HeaderSub, Util::ToWString(header.GetSubject()));

        int idx = 0;
        std::string tmppath = Util::GetTempDirectory();
        for (auto& part : body.GetParts())
        {
          if (!part.second.m_Filename.empty())
          {
            std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
            Util::MkDir(tmpfiledir);
            std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

            Util::WriteFile(tmpfilepath, part.second.m_Data);
            if (GetComposeStr(HeaderAtt).empty())
            {
              SetComposeStr(HeaderAtt, GetComposeStr(HeaderAtt) + Util::ToWString(tmpfilepath));
            }
            else
            {
              SetComposeStr(HeaderAtt, GetComposeStr(HeaderAtt) + L", " + Util::ToWString(tmpfilepath));
            }
          }
        }

        m_ComposeHeaderRef = header.GetMessageId();
        m_ComposeTempDirectory = tmppath;
      }
    }

  }
  else if (m_State == StateReplyMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    m_ComposeHeaderLine = 3;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string bodyText = m_Plaintext ? body.GetTextPlain() : body.GetTextHtml();
      std::vector<std::wstring> bodyTextLines =
        Util::WordWrap(Util::ToWString(bodyText), (m_MaxLineLength - 8), false);
      std::string indentBodyText =
        Util::AddIndent(Util::ToString(Util::Join(bodyTextLines)), "> ");

      m_ComposeMessageStr = Util::ToWString("\n\nOn " + header.GetDateTime() + " " +
                                            header.GetFrom() +
                                            " wrote:\n\n" +
                                            indentBodyText);
      Util::StripCR(m_ComposeMessageStr);

      // @todo: handle quoted commas in address name
      std::vector<std::string> tos = Util::Split(header.GetTo(), ',');
      std::vector<std::string> ccs = Util::Split(header.GetCc(), ',');

      ccs.insert(ccs.end(), tos.begin(), tos.end());
      std::string selfAddress = m_SmtpManager->GetAddress();
      for (auto it = ccs.begin(); it != ccs.end(); /* incremented in loop */)
      {
        it = ((it->find(selfAddress) == std::string::npos) &&
              (it->find(header.GetFrom()) == std::string::npos)) ? std::next(it) : ccs.erase(it);
      }

      if (!header.GetReplyTo().empty())
      {
        SetComposeStr(HeaderTo, Util::ToWString(header.GetReplyTo()));
        SetComposeStr(HeaderCc, L"");
      }
      else
      {
        SetComposeStr(HeaderTo, Util::ToWString(header.GetFrom()));
        SetComposeStr(HeaderCc, Util::ToWString(Util::Join(ccs, ", ")));
      }
      SetComposeStr(HeaderBcc, L"");
      SetComposeStr(HeaderAtt, L"");
      SetComposeStr(HeaderSub, Util::ToWString(Util::MakeReplySubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
    }

    m_IsComposeHeader = false;
  }
  else if (m_State == StateForwardMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    m_ComposeHeaderLine = 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      int idx = 0;
      std::string tmppath = Util::GetTempDirectory();
      for (auto& part : body.GetParts())
      {
        if (!part.second.m_Filename.empty())
        {
          std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
          Util::MkDir(tmpfiledir);
          std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

          Util::WriteFile(tmpfilepath, part.second.m_Data);
          if (GetComposeStr(HeaderAtt).empty())
          {
            SetComposeStr(HeaderAtt,
                          GetComposeStr(HeaderAtt) + Util::ToWString(tmpfilepath));
          }
          else
          {
            SetComposeStr(HeaderAtt,
                          GetComposeStr(HeaderAtt) + L", " + Util::ToWString(tmpfilepath));
          }
        }
      }

      m_ComposeMessageStr =
        Util::ToWString("\n\n---------- Forwarded message ---------\n"
                        "From: " + header.GetFrom() + "\n"
                        "Date: " + header.GetDateTime() + "\n"
                        "Subject: " + header.GetSubject() + "\n"
                        "To: " + header.GetTo() + "\n");
      if (!header.GetReplyTo().empty())
      {
        m_ComposeMessageStr +=
          Util::ToWString("Reply-To: " + header.GetReplyTo() + "\n");
      }

      if (!header.GetCc().empty())
      {
        m_ComposeMessageStr +=
          Util::ToWString("Cc: " + header.GetCc() + "\n");
      }

      const std::string& bodyText = m_Plaintext ? body.GetTextPlain() : body.GetTextHtml();
      m_ComposeMessageStr += Util::ToWString("\n" + bodyText);
      Util::StripCR(m_ComposeMessageStr);

      SetComposeStr(HeaderSub, Util::ToWString(Util::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
  else if (m_State == StateAddressList)
  {
    curs_set(1);
    m_AddressListFilterPos = 0;
    m_AddressListFilterStr.clear();
    m_Addresses = AddressBook::Get("");
    m_AddressListCurrentIndex = 0;
    m_AddressListCurrentAddress = "";
  }
  else if (m_State == StateFileList)
  {
    curs_set(1);
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    m_CurrentDir = Util::GetCurrentWorkingDir();
    m_Files = Util::ListPaths(m_CurrentDir);
    m_FileListCurrentIndex = 0;
    m_FileListCurrentFile.m_Name = "";
  }
  else if (m_State == StateViewPartList)
  {
    curs_set(0);
    m_PartListCurrentIndex = 0;
  }
}

void Ui::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  if (!m_Running) return;

  char uiRequest = UiRequestNone;

  bool updateIndexFromUid = false;

  if (p_Request.m_PrefetchLevel < PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Folders = p_Response.m_Folders;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new folders =", p_Response.m_Folders);
    }

    if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      size_t orgNewUidsSize = m_NewUids[p_Response.m_Folder].size();
      if (m_Uids[p_Response.m_Folder].empty())
      {
        m_NewUids[p_Response.m_Folder] = p_Response.m_Uids;
      }
      else
      {
        const std::set<uint32_t>& uids = m_Uids[p_Response.m_Folder];
        std::set<uint32_t>& newUids = m_NewUids[p_Response.m_Folder];
        for (auto& uid : p_Response.m_Uids)
        {
          if (uids.find(uid) == uids.end())
          {
            newUids.insert(uid);
          }
        }
      }

      if (!p_Response.m_Cached && (p_Response.m_Folder == m_Inbox) &&
          (m_NewUids[p_Response.m_Folder].size() > orgNewUidsSize))
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
        RemoveUidDate(p_Response.m_Folder, removedUids);
      }

      m_Uids[p_Response.m_Folder] = p_Response.m_Uids;
      uiRequest |= UiRequestDrawAll;
      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new uids =", p_Response.m_Uids);
    }

    if (!p_Request.m_GetHeaders.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Headers[p_Response.m_Folder].insert(p_Response.m_Headers.begin(), p_Response.m_Headers.end());
      uiRequest |= UiRequestDrawAll;

      AddUidDate(p_Response.m_Folder, p_Response.m_Headers);

      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new headers =", MapKey(p_Response.m_Headers));
    }

    if (!p_Request.m_GetFlags.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      std::map<uint32_t, uint32_t> newFlags = p_Response.m_Flags;
      newFlags.insert(m_Flags[p_Response.m_Folder].begin(), m_Flags[p_Response.m_Folder].end());
      m_Flags[p_Response.m_Folder] = newFlags;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new flags =", MapKey(p_Response.m_Flags));
    }

    if (!p_Request.m_GetBodys.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Bodys[p_Response.m_Folder].insert(p_Response.m_Bodys.begin(), p_Response.m_Bodys.end());
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new bodys =", MapKey(p_Response.m_Bodys));
    }
  }

  if (m_PrefetchLevel == PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      for (auto& folder : p_Response.m_Folders)
      {
        if (!m_Running)
        {
          break;
        }

        if (!m_HasRequestedUids[folder] && !m_HasPrefetchRequestedUids[folder])
        {
          ImapManager::Request request;
          request.m_PrefetchLevel = PrefetchLevelFullSync;
          request.m_Folder = folder;
          request.m_GetUids = true;
          LOG_DEBUG_VAR("prefetch request uids =", folder);
          m_HasPrefetchRequestedUids[folder] = true;
          m_ImapManager->PrefetchRequest(request);
        }
      }
    }

    if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      const std::string& folder = p_Response.m_Folder;

      std::set<uint32_t> prefetchHeaders;
      std::set<uint32_t> prefetchFlags;
      std::set<uint32_t> prefetchBodys;

      {
        std::lock_guard<std::mutex> lock(m_Mutex);

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
          if (!m_Running) break;

          subsetPrefetchHeaders.insert(*it);
          if ((subsetPrefetchHeaders.size() == maxHeadersFetchRequest) ||
              (std::next(it) == prefetchHeaders.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetHeaders = subsetPrefetchHeaders;

            LOG_DEBUG_VAR("prefetch request headers =", subsetPrefetchHeaders);
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
          if (!m_Running) break;

          subsetPrefetchFlags.insert(*it);
          if ((subsetPrefetchFlags.size() == maxFlagsFetchRequest) ||
              (std::next(it) == prefetchFlags.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetFlags = subsetPrefetchFlags;

            LOG_DEBUG_VAR("prefetch request flags =", subsetPrefetchFlags);
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
          if (!m_Running) break;

          subsetPrefetchBodys.insert(*it);
          if ((subsetPrefetchBodys.size() == maxBodysFetchRequest) ||
              (std::next(it) == prefetchBodys.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetBodys = subsetPrefetchBodys;

            LOG_DEBUG_VAR("prefetch request bodys =", subsetPrefetchBodys);
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
      SetDialogMessage("Login failed", true /* p_Warn */);
    }
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
      SetDialogMessage("Saving draft message failed", true /* p_Warn */);
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
  if (!m_DraftsFolder.empty())
  {
    SmtpManager::Action smtpAction = p_Result.m_Action;
    const std::string msg =
      (smtpAction.m_ComposeDraftUid != 0)
      ? "Send message failed. Overwrite draft (y/n)?"
      : "Send message failed. Save draft (y/n)?";
    if (PromptYesNo(msg))
    {
      smtpAction.m_IsSendMessage = false;
      smtpAction.m_IsCreateMessage = true;

      SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
      if (smtpResult.m_Result)
      {
        ImapManager::Action imapAction;
        imapAction.m_UploadDraft = true;
        imapAction.m_Folder = m_DraftsFolder;
        imapAction.m_Msg = smtpResult.m_Message;
        m_ImapManager->AsyncAction(imapAction);

        if (smtpAction.m_ComposeDraftUid != 0)
        {
          MoveMessage(smtpAction.m_ComposeDraftUid, m_DraftsFolder, m_TrashFolder);
          m_HasRequestedUids[m_TrashFolder] = false;
        }

        m_HasRequestedUids[m_DraftsFolder] = false;
      }
    }

    AsyncUiRequest(UiRequestDrawAll);
  }
  else
  {
    SetDialogMessage("Send message failed", true /* p_Warn */);
  }
}

void Ui::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    std::unique_lock<std::mutex> lock(m_SmtpErrorMutex);
    m_SmtpErrorResults.push_back(p_Result);
    AsyncUiRequest(UiRequestDrawError);
  }
  else
  {
    const SmtpManager::Action& action = p_Result.m_Action;
    const std::vector<Contact> to = Contact::FromStrings(Util::Trim(Util::Split(action.m_To)));
    const std::vector<Contact> cc = Contact::FromStrings(Util::Trim(Util::Split(action.m_Cc)));

    std::vector<Contact> contacts;
    std::move(to.begin(), to.end(), std::back_inserter(contacts));
    std::move(cc.begin(), cc.end(), std::back_inserter(contacts));

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

    if ((action.m_ComposeDraftUid != 0) && !m_DraftsFolder.empty() && !m_TrashFolder.empty())
    {
      MoveMessage(action.m_ComposeDraftUid, m_DraftsFolder, m_TrashFolder);
    }

    if (m_ClientStoreSent)
    {
      if (!m_SentFolder.empty())
      {
        ImapManager::Action imapAction;
        imapAction.m_UploadMessage = true;
        imapAction.m_Folder = m_SentFolder;
        imapAction.m_Msg = p_Result.m_Message;
        m_ImapManager->AsyncAction(imapAction);
      }
      else
      {
        SetDialogMessage("Sent folder not configured", true /* p_Warn */);
      }
    }

    if (!m_SentFolder.empty())
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_HasRequestedUids[m_SentFolder] = false;
    }
  }

  Util::RmDir(p_Result.m_Action.m_ComposeTempDirectory);
}

void Ui::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Status.Update(p_StatusUpdate);

  if (!m_HasRequestedFolders && !m_HasPrefetchRequestedFolders && (m_PrefetchLevel >= PrefetchLevelFullSync) &&
      (p_StatusUpdate.SetFlags & Status::FlagConnected))
  {
    ImapManager::Request request;
    request.m_PrefetchLevel = PrefetchLevelFullSync;
    request.m_GetFolders = true;
    LOG_DEBUG("prefetch request folders");
    m_HasPrefetchRequestedFolders = true;
    m_ImapManager->PrefetchRequest(request);
  }

  AsyncUiRequest(UiRequestDrawAll);
}

void Ui::SearchHandler(const ImapManager::SearchQuery& p_SearchQuery,
                       const ImapManager::SearchResult& p_SearchResult)
{
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
    m_MessageListSearchResultFolderUids.insert(
      m_MessageListSearchResultFolderUids.end(), p_SearchResult.m_FolderUids.begin(),
      p_SearchResult.m_FolderUids.end());
    LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
  }

  m_MessageListSearchHasMore = p_SearchResult.m_HasMore;

  AsyncUiRequest(UiRequestDrawAll);
  UpdateUidFromIndex(false /* p_UserTriggered */);
}

void Ui::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_ImapManager = p_ImapManager;
  if (m_ImapManager)
  {
    m_ImapManager->SetCurrentFolder(m_CurrentFolder);
  }
}

void Ui::SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager)
{
  m_SmtpManager = p_SmtpManager;
}

void Ui::ResetImapManager()
{
  m_ImapManager.reset();
}

void Ui::ResetSmtpManager()
{
  m_SmtpManager.reset();
}

void Ui::SetTrashFolder(const std::string& p_TrashFolder)
{
  m_TrashFolder = p_TrashFolder;
}

void Ui::SetDraftsFolder(const std::string& p_DraftsFolder)
{
  m_DraftsFolder = p_DraftsFolder;
}

void Ui::SetSentFolder(const std::string& p_SentFolder)
{
  m_SentFolder = p_SentFolder;
}

void Ui::SetClientStoreSent(bool p_ClientStoreSent)
{
  m_ClientStoreSent = p_ClientStoreSent;
}

bool Ui::IsConnected()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.IsSet(Status::FlagConnected);
}

std::string Ui::GetKeyDisplay(int p_Key)
{
  if (p_Key == '\n')
  {
    return "Re";
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
  else if (p_Key == KEY_LEFT)
  {
    return "←";
  }
  else if (p_Key == KEY_RIGHT)
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

std::string Ui::GetStatusStr()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.ToString(m_ShowProgress);
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
      else
      {
        return "Folder: " + m_CurrentFolder;
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
    case StateGotoFolder: return "Goto Folder";
    case StateMoveToFolder: return "Move To Folder";
    case StateComposeMessage: return std::string("Compose") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateReplyMessage: return std::string("Reply") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateForwardMessage: return std::string("Forward") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateAddressList: return "Address Book";
    case StateFileList: return "File Selection";
    case StateViewPartList: return "Message Parts";
    default: return "Unknown State";
  }
}

bool Ui::IsValidTextKey(int p_Key)
{
  return ((p_Key >= 0x20) || (p_Key == 0x9) || (p_Key == 0xA));
}

void Ui::SendComposedMessage()
{
  SmtpManager::Action smtpAction;
  smtpAction.m_IsSendMessage = true;
  smtpAction.m_To = Util::ToString(GetComposeStr(HeaderTo));
  smtpAction.m_Cc = Util::ToString(GetComposeStr(HeaderCc));
  smtpAction.m_Bcc = Util::ToString(GetComposeStr(HeaderBcc));
  smtpAction.m_Att = Util::ToString(GetComposeStr(HeaderAtt));
  smtpAction.m_Subject = Util::ToString(GetComposeStr(HeaderSub));
  smtpAction.m_Body = Util::ToString(m_ComposeHardwrap ? Util::Join(m_ComposeMessageLines)
                                                       : m_ComposeMessageStr);
  smtpAction.m_HtmlBody = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
  smtpAction.m_RefMsgId = m_ComposeHeaderRef;
  smtpAction.m_ComposeTempDirectory = m_ComposeTempDirectory;
  smtpAction.m_ComposeDraftUid = m_ComposeDraftUid;

  m_SmtpManager->AsyncAction(smtpAction);
}

void Ui::UploadDraftMessage()
{
  if (!m_DraftsFolder.empty())
  {
    SmtpManager::Action smtpAction;
    smtpAction.m_IsCreateMessage = true;
    smtpAction.m_To = Util::ToString(GetComposeStr(HeaderTo));
    smtpAction.m_Cc = Util::ToString(GetComposeStr(HeaderCc));
    smtpAction.m_Bcc = Util::ToString(GetComposeStr(HeaderBcc));
    smtpAction.m_Att = Util::ToString(GetComposeStr(HeaderAtt));
    smtpAction.m_Subject = Util::ToString(GetComposeStr(HeaderSub));
    smtpAction.m_Body = Util::ToString(m_ComposeHardwrap ? Util::Join(m_ComposeMessageLines)
                                                         : m_ComposeMessageStr);
    smtpAction.m_HtmlBody = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
    smtpAction.m_RefMsgId = m_ComposeHeaderRef;

    SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
    if (smtpResult.m_Result)
    {
      ImapManager::Action imapAction;
      imapAction.m_UploadDraft = true;
      imapAction.m_Folder = m_DraftsFolder;
      imapAction.m_Msg = smtpResult.m_Message;
      m_ImapManager->AsyncAction(imapAction);

      if (m_ComposeDraftUid != 0)
      {
        MoveMessage(m_ComposeDraftUid, m_DraftsFolder, m_TrashFolder);
      }
    }
  }
  else
  {
    SetDialogMessage("Drafts folder not configured", true /* p_Warn */);
  }
}

bool Ui::DeleteMessage()
{
  if (!m_TrashFolder.empty())
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    if (folder != m_TrashFolder)
    {
      if (m_DeleteWithoutConfirm || Ui::PromptYesNo("Delete message (y/n)?"))
      {
        MoveMessage(uid, folder, m_TrashFolder);

        m_MessageViewLineOffset = 0;
        UpdateUidFromIndex(true /* p_UserTriggered */);

        bool isMsgDateUidsEmpty = false;
        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          isMsgDateUidsEmpty = m_MsgDateUids[folder].empty();
        }

        if (isMsgDateUidsEmpty)
        {
          SetState(StateViewMessageList);
        }
      }
    }
    else
    {
      if (Ui::PromptYesNo("Permanently delete message (y/n)?"))
      {
        DeleteMessage(uid, folder);
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

void Ui::MoveMessage(uint32_t p_Uid, const std::string& p_From, const std::string& p_To)
{
  ImapManager::Action action;
  action.m_Folder = p_From;
  action.m_Uids.insert(p_Uid);
  action.m_MoveDestination = p_To;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;

    RemoveUidDate(folder, action.m_Uids);
    m_Uids[folder] = m_Uids[folder] - action.m_Uids;
    m_Headers[folder] = m_Headers[folder] - action.m_Uids;

    m_HasRequestedUids[p_From] = false;
    m_HasRequestedUids[p_To] = false;
  }
}

void Ui::DeleteMessage(uint32_t p_Uid, const std::string& p_Folder)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids.insert(p_Uid);
  action.m_DeleteMessages = true;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    RemoveUidDate(p_Folder, action.m_Uids);
    m_Uids[p_Folder] = m_Uids[p_Folder] - action.m_Uids;
    m_Headers[p_Folder] = m_Headers[p_Folder] - action.m_Uids;

    m_HasRequestedUids[p_Folder] = false;
  }
}

void Ui::ToggleSeen()
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

void Ui::UpdateUidFromIndex(bool p_UserTriggered)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  if (m_MessageListSearch)
  {
    const std::vector<Header>& headers = m_MessageListSearchResultHeaders;
    m_MessageListCurrentIndex[m_CurrentFolder] =
      Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)headers.size() - 1);

    const int32_t idx = m_MessageListCurrentIndex[m_CurrentFolder];
    if (idx < (int)m_MessageListSearchResultFolderUids.size())
    {
      m_CurrentFolderUid.first = m_MessageListSearchResultFolderUids[idx].first;
      m_CurrentFolderUid.second = m_MessageListSearchResultFolderUids[idx].second;
    }

    if (m_MessageListSearchHasMore &&
        ((m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight) >= ((int)headers.size())))
    {
      m_MessageListSearchOffset += m_MessageListSearchMax;
      m_MessageListSearchMax = m_MainWinHeight;
      m_MessageListSearchHasMore = false;

      ImapManager::SearchQuery searchQuery;
      searchQuery.m_QueryStr = m_MessageListSearchQuery;
      searchQuery.m_Offset = m_MessageListSearchOffset;
      searchQuery.m_Max = m_MessageListSearchMax;

      LOG_DEBUG("search str = \"%s\" offset = %d max = %d",
                searchQuery.m_QueryStr.c_str(), searchQuery.m_Offset, searchQuery.m_Max);
      m_ImapManager->AsyncSearch(searchQuery);
    }

    return;
  }

  auto& msgDateUids = m_MsgDateUids[m_CurrentFolder];

  m_MessageListCurrentIndex[m_CurrentFolder] =
    Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)msgDateUids.size() - 1);
  if (msgDateUids.size() > 0)
  {
    m_MessageListCurrentUid[m_CurrentFolder] =
      std::prev(msgDateUids.end(), m_MessageListCurrentIndex[m_CurrentFolder] + 1)->second;
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

void Ui::UpdateIndexFromUid()
{
  if (m_MessageListSearch) return;

  bool found = false;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_MessageListUidSet[m_CurrentFolder])
    {
      auto& msgDateUids = m_MsgDateUids[m_CurrentFolder];

      for (auto it = msgDateUids.rbegin(); it != msgDateUids.rend(); ++it)
      {
        if ((int32_t)it->second == m_MessageListCurrentUid[m_CurrentFolder])
        {
          m_MessageListCurrentIndex[m_CurrentFolder] = std::distance(msgDateUids.rbegin(), it);
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

  LOG_DEBUG("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder],
            m_MessageListCurrentIndex[m_CurrentFolder]);
}

void Ui::AddUidDate(const std::string& p_Folder, const std::map<uint32_t, Header>& p_UidHeaders)
{
  auto& msgDateUids = m_MsgDateUids[p_Folder];
  auto& msgUidDates = m_MsgUidDates[p_Folder];

  for (auto it = p_UidHeaders.begin(); it != p_UidHeaders.end(); ++it)
  {
    const uint32_t uid = it->first;
    const std::string& date = m_Headers[p_Folder][uid].GetDateTime();
    std::string dateUid = date + std::to_string(uid);

    if (uid == 0)
    {
      LOG_WARNING("skip add date = %s, uid = %d pair", date.c_str(), uid);
      continue;
    }

    LOG_TRACE("add date = %s, uid = %d pair", date.c_str(), uid);

    auto ret = msgDateUids.insert(std::pair<std::string, uint32_t>(dateUid, uid));
    if (ret.second)
    {
      msgUidDates.insert(std::pair<uint32_t, std::string>(uid, dateUid));
    }
  }
}

void Ui::RemoveUidDate(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  auto& msgDateUids = m_MsgDateUids[p_Folder];
  auto& msgUidDates = m_MsgUidDates[p_Folder];

  for (auto it = p_Uids.begin(); it != p_Uids.end(); ++it)
  {
    const uint32_t uid = *it;
    const std::string& date = m_Headers[p_Folder][uid].GetDateTime();
    std::string dateUid = date + std::to_string(uid);

    if (uid == 0)
    {
      LOG_WARNING("skip del date = %s, uid = %d pair", date.c_str(), uid);
      continue;
    }

    LOG_DEBUG("del date = %s, uid = %d pair", date.c_str(), uid);

    auto msgDateUid = msgDateUids.find(dateUid);
    if (msgDateUid != msgDateUids.end())
    {
      msgDateUids.erase(msgDateUid);
    }

    auto msgUidDate = msgUidDates.find(uid);
    if (msgUidDate != msgUidDates.end())
    {
      msgUidDates.erase(msgUidDate);
    }
  }
}

void Ui::ComposeMessagePrevLine()
{
  if (m_ComposeMessageWrapLine > 0)
  {
    int stepsBack = 0;
    if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() >
        m_ComposeMessageWrapPos)
    {
      stepsBack = m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() + 1;
    }
    else
    {
      stepsBack = m_ComposeMessageWrapPos + 1;
    }

    stepsBack = std::min(stepsBack, m_MaxLineLength);
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - stepsBack,
                                      (int)m_ComposeMessageStr.size());
  }
  else
  {
    m_IsComposeHeader = true;
  }
}

void Ui::ComposeMessageNextLine()
{
  if ((m_ComposeMessageWrapLine + 1) < (int)m_ComposeMessageLines.size())
  {
    int stepsForward = m_ComposeMessageLines[m_ComposeMessageWrapLine].size() -
      m_ComposeMessageWrapPos + 1;
    if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine + 1].size() >
        m_ComposeMessageWrapPos)
    {
      stepsForward += m_ComposeMessageWrapPos;
    }
    else
    {
      stepsForward += m_ComposeMessageLines[m_ComposeMessageWrapLine + 1].size();
    }

    stepsForward = std::min(stepsForward, m_MaxLineLength);
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
  else if ((int)m_ComposeMessageLines.size() > 0)
  {
    int stepsForward = m_ComposeMessageLines[m_ComposeMessageWrapLine].size() -
      m_ComposeMessageWrapPos;

    stepsForward = std::min(stepsForward, m_MaxLineLength);
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
}

int Ui::ReadKeyBlocking()
{
  while (true)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = {1, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      return key;
    }
  }
}

bool Ui::PromptYesNo(const std::string& p_Prompt)
{
  werase(m_DialogWin);

  const std::string& dispStr = p_Prompt;
  int x = std::max((m_ScreenWidth - (int)dispStr.size() - 1) / 2, 0);
  wattron(m_DialogWin, A_REVERSE);
  mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
  wattroff(m_DialogWin, A_REVERSE);

  wrefresh(m_DialogWin);

  int key = ReadKeyBlocking();

  return ((key == 'y') || (key == 'Y'));
}

bool Ui::PromptString(const std::string& p_Prompt, const std::string& p_Action,
                      std::string& p_Entry)
{
  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    static std::vector<std::vector<std::string>> savePartHelp =
    {
      {
        GetKeyDisplay(KEY_RETURN), p_Action,
      },
      {
        GetKeyDisplay(m_KeyCancel), "Cancel",
      }
    };

    DrawHelpText(savePartHelp);

    wrefresh(m_HelpWin);
  }

  curs_set(1);

  m_FilenameEntryString = Util::ToWString(p_Entry);
  m_FilenameEntryStringPos = m_FilenameEntryString.size();

  bool rv = false;
  while (true)
  {
    werase(m_DialogWin);

    const std::string& dispStr = p_Prompt + Util::ToString(m_FilenameEntryString);
    mvwprintw(m_DialogWin, 0, 3, "%s", dispStr.c_str());

    leaveok(m_DialogWin, false);
    wmove(m_DialogWin, 0, 3 + p_Prompt.size() + m_FilenameEntryStringPos);
    wrefresh(m_DialogWin);
    leaveok(m_DialogWin, true);

    int key = ReadKeyBlocking();
    if (key == m_KeyCancel)
    {
      rv = false;
      break;
    }
    else if ((key == KEY_RETURN) || (key == KEY_ENTER))
    {
      p_Entry = Util::ToString(m_FilenameEntryString);
      rv = true;
      break;
    }
    else if (key == KEY_LEFT)
    {
      m_FilenameEntryStringPos = Util::Bound(0, m_FilenameEntryStringPos - 1,
                                             (int)m_FilenameEntryString.size());
    }
    else if (key == KEY_RIGHT)
    {
      m_FilenameEntryStringPos = Util::Bound(0, m_FilenameEntryStringPos + 1,
                                             (int)m_FilenameEntryString.size());
    }
    else if ((key == KEY_UP) || (key == KEY_DOWN) ||
             (key == m_KeyPrevPage) || (key == m_KeyNextPage) ||
             (key == KEY_HOME) || (key == KEY_END))
    {
      // ignore
    }
    else if ((key == KEY_BACKSPACE) || (key == KEY_DELETE))
    {
      if (m_FilenameEntryStringPos > 0)
      {
        m_FilenameEntryString.erase(--m_FilenameEntryStringPos, 1);
      }
    }
    else if (key == KEY_DC)
    {
      if (m_FilenameEntryStringPos < (int)m_FilenameEntryString.size())
      {
        m_FilenameEntryString.erase(m_FilenameEntryStringPos, 1);
      }
    }
    else if (IsValidTextKey(key))
    {
      m_FilenameEntryString.insert(m_FilenameEntryStringPos++, 1, key);
    }
  }

  curs_set(0);
  return rv;
}

bool Ui::CurrentMessageBodyAvailable()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
  std::map<uint32_t, Body>::const_iterator bit = bodys.find(uid);
  return (bit != bodys.end());
}

void Ui::InvalidateUiCache(const std::string& p_Folder)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_HasRequestedUids[p_Folder] = false;
  m_Flags[p_Folder].clear();
  m_RequestedFlags[p_Folder].clear();
}

void Ui::ExtEditor(std::wstring& p_ComposeMessageStr, int& p_ComposeMessagePos)
{
  endwin();
  const std::string& tempPath = Util::GetTempFilename(".txt");
  Util::WriteWFile(tempPath, p_ComposeMessageStr);
  const std::string& editor = Util::GetEditorCmd();
  const std::string& cmd = editor + " " + tempPath;
  LOG_DEBUG("launching external editor: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("external editor exited successfully");
    p_ComposeMessageStr = Util::ReadWFile(tempPath);
    p_ComposeMessagePos = 0;
  }
  else
  {
    LOG_WARNING("external editor exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  Util::DeleteFile(tempPath);

  refresh();

  wint_t key = 0;
  while (get_wch(&key) != ERR)
  {
    // Discard any remaining input
  }
}

void Ui::ExtPager()
{
  endwin();
  const std::string& tempPath = Util::GetTempFilename(".txt");
  Util::WriteFile(tempPath, m_CurrentMessageViewText);
  const std::string& pager = Util::GetPagerCmd();
  const std::string& cmd = pager + " " + tempPath;
  LOG_DEBUG("launching external pager: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("external pager exited successfully");
  }
  else
  {
    LOG_WARNING("external pager exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  Util::DeleteFile(tempPath);

  refresh();

  wint_t key = 0;
  while (get_wch(&key) != ERR)
  {
    // Discard any remaining input
  }
}

int Ui::ExtPartsViewer(const std::string& p_Path)
{
  const bool isDefaultPartsViewerCmd = Util::IsDefaultPartsViewerCmd();
  if (!isDefaultPartsViewerCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetPartsViewerCmd();
  const std::string& cmd = viewer + " \"" + p_Path + "\"";
  LOG_DEBUG("launching external viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("external viewer exited successfully");
  }
  else
  {
    LOG_WARNING("external viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultPartsViewerCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }

  return rv;
}

void Ui::ExtHtmlViewer()
{
  static const std::string& tempPath = Util::GetTempDir() + std::string("htmlview/tmp.html");
  Util::DeleteFile(tempPath);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& html = body.GetHtml(); // falls back to text/plain if no html
      Util::WriteFile(tempPath, html);
    }
  }

  if (Util::Exists(tempPath))
  {
    ExtHtmlViewer(tempPath);
  }
  else
  {
    SetDialogMessage("View html failed (message not available)", true /* p_Warn */);
  }
}

int Ui::ExtHtmlViewer(const std::string& p_Path)
{
  const bool isDefaultHtmlViewerCmd = Util::IsDefaultHtmlViewerCmd();
  if (!isDefaultHtmlViewerCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetHtmlViewerCmd();
  const std::string& cmd = viewer + " \"" + p_Path + "\"";
  LOG_DEBUG("launching html viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("html viewer exited successfully");
  }
  else
  {
    LOG_WARNING("html viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultHtmlViewerCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }

  return rv;
}

void Ui::ExtMsgViewer()
{
  static const std::string& tempPath = Util::GetTempDir() + std::string("msgview/tmp.eml");
  Util::DeleteFile(tempPath);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& data = body.GetData(); // falls back to text/plain if no html
      Util::WriteFile(tempPath, data);
    }
  }

  if (Util::Exists(tempPath))
  {
    ExtMsgViewer(tempPath);
  }
  else
  {
    SetDialogMessage("View message failed (message not available)", true /* p_Warn */);
  }
}

void Ui::ExtMsgViewer(const std::string& p_Path)
{
  const bool isDefaultMsgViewerCmd = Util::IsDefaultMsgViewerCmd();
  if (!isDefaultMsgViewerCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetMsgViewerCmd();
  const std::string& cmd = viewer + " \"" + p_Path + "\"";
  LOG_DEBUG("launching message viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("message viewer exited successfully");
  }
  else
  {
    LOG_WARNING("message viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultMsgViewerCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }
}

void Ui::SetLastStateOrMessageList()
{
  bool isMsgDateUidsEmpty = false;
  if (m_MessageListSearch)
  {
    isMsgDateUidsEmpty = false;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    isMsgDateUidsEmpty = m_MsgDateUids[m_CurrentFolder].empty();
  }

  if (isMsgDateUidsEmpty)
  {
    SetState(StateViewMessageList);
  }
  else
  {
    SetState(m_LastState);
  }
}

void Ui::ExportMessage()
{
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::string filename = std::to_string(uid) + ".eml";
  if (PromptString("Export Filename: ", "Save", filename))
  {
    if (!filename.empty())
    {
      filename = Util::ExpandPath(filename);
      std::unique_lock<std::mutex> lock(m_Mutex);
      const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      if (bodys.find(uid) != bodys.end())
      {
        Util::WriteFile(filename, m_Bodys[folder][uid].GetData());
        lock.unlock();
        SetDialogMessage("Message exported");
      }
      else
      {
        lock.unlock();
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

void Ui::ImportMessage()
{
  std::string filename;
  if (PromptString("Import Filename: ", "Load", filename))
  {
    if (!filename.empty())
    {
      filename = Util::ExpandPath(filename);
      if (Util::NotEmpty(filename))
      {
        const std::string& msg = Util::ReadFile(filename);

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

void Ui::SearchMessage()
{
  std::string query = (m_MessageListSearch && m_PersistSearchQuery) ? m_MessageListSearchQuery : "";
  if (PromptString("Search Emails: ", "Search", query))
  {
    if (!query.empty())
    {
      m_MessageListSearch = true;
      if (m_CurrentFolder != "")
      {
        m_PreviousFolder = m_CurrentFolder;
        m_CurrentFolder = "";
      }

      m_MessageListCurrentIndex[m_CurrentFolder] = 0;

      m_MessageListSearchQuery = query;
      m_MessageListSearchOffset = 0;
      m_MessageListSearchMax = m_MainWinHeight + m_MainWinHeight;
      m_MessageListSearchHasMore = false;
      m_MessageListSearchResultHeaders.clear();
      m_MessageListSearchResultFolderUids.clear();

      ImapManager::SearchQuery searchQuery;
      searchQuery.m_QueryStr = query;
      searchQuery.m_Offset = 0;
      searchQuery.m_Max = 2 * m_MainWinHeight;

      LOG_DEBUG("search str=\"%s\" offset=%d max=%d",
                searchQuery.m_QueryStr.c_str(), searchQuery.m_Offset, searchQuery.m_Max);
      m_ImapManager->AsyncSearch(searchQuery);
    }
    else
    {
      m_MessageListSearch = false;
      if (m_PreviousFolder != "")
      {
        m_CurrentFolder = m_PreviousFolder;
        m_PreviousFolder = "";
      }

      UpdateIndexFromUid();
    }
  }
}

void Ui::Quit()
{
  if (m_QuitWithoutConfirm || Ui::PromptYesNo("Quit nmail (y/n)?"))
  {
    m_Running = false;
    LOG_DEBUG("stop thread");
  }
}

std::wstring Ui::GetComposeStr(int p_HeaderField)
{
  if (m_ShowRichHeader)
  {
    return m_ComposeHeaderStr[p_HeaderField];
  }
  else
  {
    switch (p_HeaderField)
    {
      case HeaderTo: return m_ComposeHeaderStr[0];

      case HeaderCc: return m_ComposeHeaderStr[1];

      case HeaderAtt: return m_ComposeHeaderStr[2];

      case HeaderSub: return m_ComposeHeaderStr[3];

      default: break;
    }
  }

  return L"";
}

void Ui::SetComposeStr(int p_HeaderField, const std::wstring& p_Str)
{
  if (p_HeaderField == HeaderAll)
  {
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = p_Str;
    m_ComposeHeaderStr[1] = p_Str;
    m_ComposeHeaderStr[2] = p_Str;
    m_ComposeHeaderStr[3] = p_Str;
    if (m_ShowRichHeader)
    {
      m_ComposeHeaderStr[4] = p_Str;
    }
  }
  else
  {
    if (m_ShowRichHeader)
    {
      m_ComposeHeaderStr[p_HeaderField] = p_Str;
    }
    else
    {
      switch (p_HeaderField)
      {
        case HeaderTo:
          m_ComposeHeaderStr[0] = p_Str;
          break;

        case HeaderCc:
          m_ComposeHeaderStr[1] = p_Str;
          break;

        case HeaderAtt:
          m_ComposeHeaderStr[2] = p_Str;
          break;

        case HeaderSub:
          m_ComposeHeaderStr[3] = p_Str;
          break;

        default:
          break;
      }
    }
  }
}

int Ui::GetCurrentHeaderField()
{
  if (m_ShowRichHeader)
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return HeaderTo;

      case 1: return HeaderCc;

      case 2: return HeaderBcc;

      case 3: return HeaderAtt;

      case 4: return HeaderSub;

      default: break;
    }
  }
  else
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return HeaderTo;

      case 1: return HeaderCc;

      case 2: return HeaderAtt;

      case 3: return HeaderSub;

      default: break;
    }
  }

  return HeaderAll;
}

void Ui::StartSync()
{
  if (IsConnected())
  {
    if (m_PrefetchLevel < PrefetchLevelFullSync)
    {
      LOG_DEBUG("manual full sync started");
      m_PrefetchLevel = PrefetchLevelFullSync;

      ImapManager::Request request;
      request.m_PrefetchLevel = PrefetchLevelFullSync;
      request.m_GetFolders = true;
      LOG_DEBUG("prefetch request folders");
      m_HasPrefetchRequestedFolders = true;
      m_ImapManager->PrefetchRequest(request);
    }
    else
    {
      SetDialogMessage("Sync already enabled", true /* p_Warn */);
    }
  }
  else
  {
    SetDialogMessage("Cannot sync while offline", true /* p_Warn */);
  }
}

std::string Ui::MakeHtmlPart(const std::string& p_Text)
{
  if (!m_CurrentMarkdownHtmlCompose) return std::string();

  return Util::ConvertTextToHtml(p_Text);
}
