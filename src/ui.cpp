// ui.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <sstream>

#include "flag.h"
#include "loghelp.h"
#include "maphelp.h"
#include "sethelp.h"
#include "status.h"

Ui::Ui()
{
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
  pipe(m_Pipe);

  const std::map<std::string, std::string> defaultConfig =
  {
    {"help_enabled", "1"},
    {"persist_folder_filter", "1"},
    {"plain_text", "1"},
    {"key_prev_msg", "p"},
    {"key_next_msg", "n"},
    {"key_reply", "r"},
    {"key_forward", "f"},
    {"key_delete", "d"},
    {"key_compose", "c"},
    {"key_toggle_unread", "u"},
    {"key_move", "m"},
    {"key_refresh", "l"},
    {"key_quit", "q"},
    {"key_toggle_text_html", "t"},
    {"key_cancel", "KEY_CTRLC"},
    {"key_send", "KEY_CTRLX"},
    {"key_delete_line", "KEY_CTRLK"},
    {"key_open", "."},
    {"key_back", ","},
    {"key_goto_folder", "g"},
  };
  const std::string configPath(Util::GetApplicationDir() + std::string("ui.conf"));
  m_Config = Config(configPath, defaultConfig);

  m_HelpEnabled = m_Config.Get("help_enabled") == "1";
  m_PersistFolderFilter = m_Config.Get("persist_folder_filter") == "1";
  m_Plaintext = m_Config.Get("plain_text") == "1";
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
}

void Ui::Cleanup()
{
  m_Config.Set("plain_text", m_Plaintext ? "1" : "0");
  m_Config.Save();
  close(m_Pipe[0]);
  close(m_Pipe[1]);
  wclear(stdscr);
  endwin();
}

void Ui::InitWindows()
{
  getmaxyx(stdscr, m_ScreenHeight, m_ScreenWidth);
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
      DrawMessageList();
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

  std::string version = Util::GetAppVersion();
  std::string topLeft = Util::TrimPadString("  nmail " + version, 32);
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
      DrawFolderListDialog();
      break;

    default:
      DrawDefaultDialog();
      break;
  }
}

void Ui::DrawFolderListDialog()
{
  werase(m_DialogWin);
  const std::string& dispStr = Util::ToString(m_DialogEntryString);
  mvwprintw(m_DialogWin, 0, 0, "   Search: %s", dispStr.c_str());

  leaveok(m_DialogWin, false);
  wmove(m_DialogWin, 0, 11 + m_DialogEntryStringPos);
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

void Ui::SetDialogMessage(const std::string &p_DialogMessage)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_DialogMessage = p_DialogMessage;
  m_DialogMessageTime = std::chrono::system_clock::now();
}

void Ui::DrawHelp()
{
  static std::vector<std::vector<std::string>> viewMessagesListHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "Folders",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReply), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyRefresh), "Refresh",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewMsg",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
  };

  static std::vector<std::vector<std::string>> viewMessageHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "MsgList",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReply), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyToggleTextHtml), "TgTxtHtml",
    },
    {
      "",  "",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    }
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
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
    },
  };

  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    switch (m_State)
    {
      case StateViewMessageList:
        DrawHelpText(viewMessagesListHelp);
        break;

      case StateViewMessage:
        DrawHelpText(viewMessageHelp);
        break;

      case StateGotoFolder:
      case StateMoveToFolder:
        DrawHelpText(viewFoldersHelp);
        break;

      case StateComposeMessage:
      case StateReplyMessage:
      case StateForwardMessage:
        DrawHelpText(composeMessageHelp);
        break;

      default:
        break;
    }

    wrefresh(m_HelpWin);
  }
}

void Ui::DrawHelpText(const std::vector<std::vector<std::string> > &p_HelpText)
{
  int cols = 6;
  int width = m_ScreenWidth / cols;

  int y = 0;
  for (auto rowIt = p_HelpText.begin(); rowIt != p_HelpText.end(); ++rowIt)
  {
    int x = 0;
    for (int colIdx = 0; colIdx < (int)rowIt->size(); colIdx += 2)
    {
      std::string cmd = rowIt->at(colIdx);
      std::string desc = rowIt->at(colIdx + 1);

      wattron(m_HelpWin, A_REVERSE);
      mvwprintw(m_HelpWin, y, x, "%s", cmd.c_str());
      wattroff(m_HelpWin, A_REVERSE);

      const std::string descTrim = desc.substr(0, width - cmd.size() - 2);
      mvwprintw(m_HelpWin, y, x + cmd.size() + 1, "%s", descTrim.c_str());

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
    m_ImapManager->AsyncRequest(request);
    m_HasRequestedFolders = true;
  }

  werase(m_MainWin);

  std::set<std::string> folders;

  if (m_DialogEntryString.empty())
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    folders = m_Folders;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& folder : m_Folders)
    {
      if (Util::ToLower(folder).find(Util::ToLower(Util::ToString(m_DialogEntryString)))
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

      mvwprintw(m_MainWin, i - idxOffs, 2, "%s", folder.c_str());

      if (i == m_FolderListCurrentIndex)
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
    m_ImapManager->AsyncRequest(request);
    m_HasRequestedUids[m_CurrentFolder] = true;
  }

  std::set<uint32_t> uids;
  std::map<uint32_t, Header> headers;
  std::map<uint32_t, uint32_t> flags;
  std::vector<std::pair<uint32_t, Header>> msgList;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    uids = m_Uids[m_CurrentFolder];
    headers = m_Headers[m_CurrentFolder];
    flags = m_Flags[m_CurrentFolder];
    msgList = m_MsgList[m_CurrentFolder];
  }

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[m_CurrentFolder];
  for (auto& uid : uids)
  {
    if ((headers.find(uid) == headers.end()) &&
        (requestedHeaders.find(uid) == requestedHeaders.end()))
    {
      LOG_DEBUG("fetch header %d", uid);
      fetchHeaderUids.insert(uid);
      requestedHeaders.insert(uid);
    }
  }

  const int maxHeadersFetchRequest = 20;
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
        request.m_GetFlags = subsetFetchHeaderUids;

        LOG_DEBUG("fetch headers = %d", subsetFetchHeaderUids.size());
        m_ImapManager->AsyncRequest(request);
        
        subsetFetchHeaderUids.clear(); 
      }
    }
  }

  int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex - ((m_MainWinHeight - 1) / 2)),
                            std::max(0, (int)msgList.size() - (int)m_MainWinHeight));
  int idxMax = idxOffs + std::min(m_MainWinHeight, (int)msgList.size());

  werase(m_MainWin);

  for (int i = idxOffs; i < idxMax; ++i)
  {
    uint32_t uid = std::prev(msgList.end(), i + 1)->first;

    std::string seenFlag;
    if ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))))
    {
      seenFlag = std::string("N");
    }

    std::string shortDate;
    std::string shortFrom;
    std::string subject;
    if (headers.find(uid) != headers.end())
    {
      const Header& header = headers.at(uid);
      shortDate = header.GetShortDate();
      shortFrom = header.GetShortFrom();
      subject = header.GetSubject();
    }

    seenFlag = Util::TrimPadString(seenFlag, 1);
    shortDate = Util::TrimPadString(shortDate, 10);
    shortFrom = Util::ToString(Util::TrimPadWString(Util::ToWString(shortFrom), 20));
    std::string headerLeft = " " + seenFlag + "  " + shortDate + "  " + shortFrom + "  ";
    int subjectWidth = m_ScreenWidth - headerLeft.size() - 1;
    subject = Util::ToString(Util::TrimPadWString(Util::ToWString(subject), subjectWidth));
    std::string header = headerLeft + subject + " ";

    if (i == m_MessageListCurrentIndex)
    {
      wattron(m_MainWin, A_REVERSE);
    }

    mvwprintw(m_MainWin, i - idxOffs, 0, "%s", header.c_str());

    if (i == m_MessageListCurrentIndex)
    {
      wattroff(m_MainWin, A_REVERSE);
    }

    if (i == m_MessageListCurrentIndex)
    {
      m_MessageListCurrentUid = uid;
    }

    // @todo: consider add option for prefetching of all bodys of listed messages.
    if (i == m_MessageListCurrentIndex)
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      const std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];
      std::set<uint32_t>& prefetchedBodys = m_PrefetchedBodys[m_CurrentFolder];
      std::set<uint32_t>& requestedBodys = m_RequestedBodys[m_CurrentFolder];
      if ((bodys.find(uid) == bodys.end()) &&
          (prefetchedBodys.find(uid) == prefetchedBodys.end()) &&
          (requestedBodys.find(uid) == requestedBodys.end()))
      {
        prefetchedBodys.insert(uid);

        std::set<uint32_t> fetchBodyUids;
        fetchBodyUids.insert(uid);

        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetBodys = fetchBodyUids;

        m_ImapManager->PrefetchRequest(request);
      }

    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessage()
{
  werase(m_MainWin);

  std::set<uint32_t> fetchBodyUids;
  bool markSeen = false;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];

    std::set<uint32_t>& requestedBodys = m_RequestedBodys[m_CurrentFolder];

    int uid = m_MessageListCurrentUid;

    if ((bodys.find(uid) == bodys.end()) &&
        (requestedBodys.find(uid) == requestedBodys.end()))
    {
      requestedBodys.insert(uid);
      fetchBodyUids.insert(uid);
    }

    std::string headerText;
    std::map<uint32_t, Header>::const_iterator headerIt = headers.find(uid);
    std::stringstream ss;
    if (headerIt != headers.end())
    {
      const Header& header = headerIt->second;
      ss << "Date: " << header.GetDate() << "\n";
      ss << "From: " << header.GetFrom() << "\n";
      ss << "To: " << header.GetTo() << "\n";
      if (!header.GetCc().empty())
      {
        ss << "Cc: " << header.GetCc() << "\n";
      }

      ss << "Subject: " << header.GetSubject() << "\n";
      ss << "\n";
    }

    headerText = ss.str();

    std::map<uint32_t, Body>::const_iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      const Body& body = bodyIt->second;
      const std::string& bodyText = m_Plaintext ? body.GetTextPlain() : body.GetText();
      const std::string text = headerText + bodyText;
      const std::vector<std::string>& lines = Util::WordWrap(text, m_ScreenWidth);
      int countLines = lines.size();

      m_MessageViewLineOffset = Util::Bound(0, m_MessageViewLineOffset,
                                            countLines - m_MainWinHeight);
      for (int i = 0; ((i < m_MainWinHeight) && (i < countLines)); ++i)
      {
        const std::string& dispStr = lines.at(i + m_MessageViewLineOffset);
        mvwprintw(m_MainWin, i, 0, "%s", dispStr.c_str());
      }

      markSeen = true;
    }
  }

  if (!fetchBodyUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetBodys = fetchBodyUids;
    m_ImapManager->AsyncRequest(request);
  }

  if (markSeen)
  {
    MarkSeen();
  }
  
  wrefresh(m_MainWin);
}

void Ui::DrawComposeMessage()
{
  m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_ScreenWidth,
                                         m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                         m_ComposeMessageWrapPos);

  int cursY = 0;
  int cursX = 0;
  if (m_IsComposeHeader)
  {
    if (m_ComposeHeaderLine < 4)
    {
      cursY = m_ComposeHeaderLine;
      cursX = m_ComposeHeaderPos + 10;
    }
    else
    {
      cursY = m_ComposeHeaderLine + 1;
      cursX = m_ComposeHeaderPos;
    }
  }
  else
  {
    cursY = 5 + m_ComposeMessageWrapLine;
    cursX = m_ComposeMessageWrapPos;
  }

  werase(m_MainWin);

  std::vector<std::wstring> composeLines;
  composeLines.push_back(std::wstring(L"To      : ") + m_ComposeHeaderStr.at(0));
  composeLines.push_back(std::wstring(L"Cc      : ") + m_ComposeHeaderStr.at(1));
  composeLines.push_back(std::wstring(L"Attchmnt: ") + m_ComposeHeaderStr.at(2));
  composeLines.push_back(std::wstring(L"Subject : ") + m_ComposeHeaderStr.at(3));
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

void Ui::AsyncDrawRequest(char p_DrawRequest)
{
  write(m_Pipe[1], &p_DrawRequest, 1);
}

void Ui::PerformDrawRequest(char p_DrawRequest)
{
  if (p_DrawRequest & DrawRequestAll)
  {
    DrawAll();
  }
}

void Ui::Run()
{
  DrawAll();
  m_Running = true;
  LOG_DEBUG("entering ui loop");

  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = std::max(STDIN_FILENO, m_Pipe[0]);
    struct timeval tv = {1, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        read(m_Pipe[0], &buf[0], len);
        char drawRequest = DrawRequestNone;
        for (int i = 0; i < len; ++i)
        {
          drawRequest |= buf[i];
        }

        PerformDrawRequest(drawRequest);
      }
    }

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      if (key == KEY_RESIZE)
      {
        CleanupWindows();
        InitWindows();
        DrawAll();
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

        default:
          break;
      }

    }
  }

  LOG_DEBUG("exiting ui loop");
  
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
  else if (p_Key == KEY_PPAGE)
  {
    m_FolderListCurrentIndex = m_FolderListCurrentIndex - m_MainWinHeight;
  }
  else if (p_Key == KEY_NPAGE)
  {
    m_FolderListCurrentIndex = m_FolderListCurrentIndex + m_MainWinHeight;
  }
  else if (p_Key == KEY_RETURN)
  {
    if (m_State == StateGotoFolder)
    {
      m_CurrentFolder = m_FolderListCurrentFolder;
      m_ImapManager->SetCurrentFolder(m_CurrentFolder);
      SetState(StateViewMessageList);
      UpdateCurrentUid();
    }
    else if (m_State == StateMoveToFolder)
    {
      if (m_FolderListCurrentFolder != m_CurrentFolder)
      {
        ImapManager::Action action;
        action.m_Folder = m_CurrentFolder;
        action.m_Uids.insert(m_MessageListCurrentUid);
        action.m_MoveDestination = m_FolderListCurrentFolder;
        m_ImapManager->AsyncAction(action);

        m_HasRequestedUids[m_FolderListCurrentFolder] = false;

        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          m_Uids[m_CurrentFolder] = m_Uids[m_CurrentFolder] - action.m_Uids;
          m_Uids[m_FolderListCurrentFolder] = m_Uids[m_FolderListCurrentFolder] + action.m_Uids;
          m_Headers[m_CurrentFolder] = m_Headers[m_CurrentFolder] - action.m_Uids;
          UpdateMsgList(m_CurrentFolder);
          UpdateMsgList(m_FolderListCurrentFolder);
        }

        std::vector<std::pair<uint32_t, Header>> msgList;
        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          msgList = m_MsgList[m_CurrentFolder];
        }

        UpdateCurrentUid();
        if (msgList.empty())
        {
          SetState(StateViewMessageList);
        }
        else
        {
          SetState(m_LastState);
        }
      }
      else
      {
        SetDialogMessage("Move to same folder ignored");
        UpdateCurrentUid();
        SetState(m_LastState);
      }
    }
  }
  else if (p_Key == KEY_LEFT)
  {
    m_DialogEntryStringPos = Util::Bound(0, m_DialogEntryStringPos - 1,
                                         (int)m_DialogEntryString.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    m_DialogEntryStringPos = Util::Bound(0, m_DialogEntryStringPos + 1,
                                         (int)m_DialogEntryString.size());
  }
  else if (p_Key == KEY_SYS_BACKSPACE)
  {
    if (m_DialogEntryStringPos > 0)
    {
      m_DialogEntryString.erase(--m_DialogEntryStringPos, 1);
    }
  }
  else if (p_Key == KEY_DC)
  {
    if (m_DialogEntryStringPos < (int)m_DialogEntryString.size())
    {
      m_DialogEntryString.erase(m_DialogEntryStringPos, 1);
    }
  }
  else if (IsValidTextKey(p_Key))
  {
    m_DialogEntryString.insert(m_DialogEntryStringPos++, 1, p_Key);
  }

  DrawAll();
}

void Ui::ViewMessageListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    m_Running = false;
    LOG_DEBUG("ui running flag set false");
  }
  else if (p_Key == m_KeyRefresh)
  {
    if (IsConnected())
    {
      m_HasRequestedUids[m_CurrentFolder] = false;
    }
    else
    {
      SetDialogMessage("Cannot refresh while offline");
    }
  }
  else if ((p_Key == KEY_UP) || (p_Key == m_KeyPrevMsg))
  {
    --m_MessageListCurrentIndex;
    UpdateCurrentUid();
  }
  else if ((p_Key == KEY_DOWN) || (p_Key == m_KeyNextMsg))
  {
    ++m_MessageListCurrentIndex;
    UpdateCurrentUid();
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_MessageListCurrentIndex = m_MessageListCurrentIndex - m_MainWinHeight;
    UpdateCurrentUid();
  }
  else if ((p_Key == KEY_NPAGE) || (p_Key == KEY_SPACE))
  {
    m_MessageListCurrentIndex = m_MessageListCurrentIndex + m_MainWinHeight;
    UpdateCurrentUid();
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == m_KeyOpen))
  {
    SetState(StateViewMessage);
  }
  else if ((p_Key == m_KeyGotoFolder) || (p_Key == m_KeyBack))
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
      SetState(StateReplyMessage);
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
      SetState(StateForwardMessage);
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
      ToggleUnseen();
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
  }

  DrawAll();
}

void Ui::ViewMessageKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    m_Running = false;
    LOG_DEBUG("running flag set false");
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
    --m_MessageListCurrentIndex;
    UpdateCurrentUid();
  }
  else if (p_Key == m_KeyNextMsg)
  {
    ++m_MessageListCurrentIndex;
    UpdateCurrentUid();
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset - m_MainWinHeight;
  }
  else if ((p_Key == KEY_NPAGE) || (p_Key == KEY_SPACE))
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset + m_MainWinHeight;
  }
  else if ((p_Key == KEY_SYS_BACKSPACE) || (p_Key == m_KeyBack))
  {
    SetState(StateViewMessageList);
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
      SetState(StateReplyMessage);
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
      SetState(StateForwardMessage);
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
      ToggleUnseen();
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
  }

  DrawAll();
}

void Ui::ComposeMessageKeyHandler(int p_Key)
{
  if (m_IsComposeHeader)
  {
    if (p_Key == m_KeyCancel)
    {
      SetState(m_LastState);
    }
    else if (p_Key == m_KeySend)
    {
      SendComposedMessage();
      SetState(m_LastState);
    }
    else if (p_Key == KEY_UP)
    {
      m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine - 1,
                                        (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == KEY_DOWN) || (p_Key == KEY_RETURN) || (p_Key == KEY_TAB))
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
    else if (p_Key == KEY_PPAGE)
    {
      m_ComposeHeaderLine = 0;
      m_ComposeHeaderPos = 0;
    }
    else if (p_Key == KEY_NPAGE)
    {
      m_IsComposeHeader = false;
    }
    else if (p_Key == KEY_LEFT)
    {
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos - 1,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if (p_Key == KEY_RIGHT)
    {
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos + 1,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }    
    else if (p_Key == KEY_SYS_BACKSPACE)
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
    else if (IsValidTextKey(p_Key))
    {
      m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos++, 1, p_Key);
    }
    else
    {
      SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
    }
  }
  else
  {
    if (p_Key == m_KeyCancel)
    {
      SetState(m_LastState);
    }
    else if (p_Key == m_KeySend)
    {
      SendComposedMessage();
      SetState(m_LastState);
    }
    else if (p_Key == KEY_UP)
    {
      ComposeMessagePrevLine();
    }
    else if (p_Key == KEY_DOWN)
    {
      ComposeMessageNextLine();
    }
    else if (p_Key == KEY_PPAGE)
    {
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessagePrevLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_ScreenWidth,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == KEY_NPAGE)
    {
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessageNextLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_ScreenWidth,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == KEY_LEFT)
    {
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - 1,
                                        (int)m_ComposeMessageStr.size());
    }
    else if (p_Key == KEY_RIGHT)
    {
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + 1,
                                        (int)m_ComposeMessageStr.size());
    }
    else if (p_Key == KEY_SYS_BACKSPACE)
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
    else if (IsValidTextKey(p_Key))
    {
      m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, p_Key);
    }
    else
    {
      SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
    }
  }

  DrawAll();
}

void Ui::SetState(Ui::State p_State)
{
  m_LastState = m_State;
  m_State = p_State;

  if (m_State == StateGotoFolder)
  {
    curs_set(1);
    m_DialogEntryStringPos = 0;
    m_DialogEntryString.clear();
    m_FolderListCurrentFolder = m_CurrentFolder;
    m_FolderListCurrentIndex = INT_MAX;
  }
  else if (m_State == StateMoveToFolder)
  {
    curs_set(1);
    if (!m_PersistFolderFilter)
    {
      m_DialogEntryStringPos = 0;
      m_DialogEntryString.clear();
      m_FolderListCurrentFolder = m_CurrentFolder;
      m_FolderListCurrentIndex = INT_MAX;
    }
  }
  else if (m_State == StateViewMessageList)
  {
    curs_set(0);
  }
  else if (m_State == StateViewMessage)
  {
    curs_set(0);
    m_MessageViewLineOffset = 0;
  }
  else if (m_State == StateComposeMessage)
  {
    curs_set(1);
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = L"";
    m_ComposeHeaderStr[1] = L"";
    m_ComposeHeaderStr[2] = L"";
    m_ComposeHeaderStr[3] = L"";
    m_ComposeHeaderLine = 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_IsComposeHeader = true;
  }
  else if (m_State == StateReplyMessage)
  {
    curs_set(1);
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = L"";
    m_ComposeHeaderStr[1] = L"";
    m_ComposeHeaderStr[2] = L"";
    m_ComposeHeaderStr[3] = L"";
    m_ComposeHeaderLine = 3;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    // m_MessageListCurrentUid
    std::map<uint32_t, Header> headers;
    std::map<uint32_t, Body> bodys;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      headers = m_Headers[m_CurrentFolder];
      bodys = m_Bodys[m_CurrentFolder];
    }

    std::map<uint32_t, Header>::const_iterator hit = headers.find(m_MessageListCurrentUid);
    std::map<uint32_t, Body>::const_iterator bit = bodys.find(m_MessageListCurrentUid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      const Header& header = hit->second;
      const Body& body = bit->second;

      m_ComposeMessageStr = Util::ToWString("\n\nOn " + header.GetDate() + " " +
                                            header.GetFrom() +
                                            " wrote:\n\n" +
                                            Util::AddIndent(body.GetTextPlain(), "> "));

      // @todo: handle quoted commas in address name
      std::vector<std::string> ccs = Util::Split(header.GetCc(), ',');
      std::vector<std::string> tos = Util::Split(header.GetTo(), ',');
      ccs.insert(ccs.end(), tos.begin(), tos.end());
      std::string selfAddress = m_SmtpManager->GetAddress();
      for (auto it = ccs.begin(); it != ccs.end(); /* incremented in loop */)
      {
        it = ((it->find(selfAddress) == std::string::npos) &&
              (it->find(header.GetFrom()) == std::string::npos)) ? std::next(it) : ccs.erase(it);
      }
      
      m_ComposeHeaderStr[0] = Util::ToWString(header.GetFrom());
      m_ComposeHeaderStr[1] = Util::ToWString(Util::Join(ccs, ", "));
      m_ComposeHeaderStr[2] = L"";
      m_ComposeHeaderStr[3] = Util::ToWString(Util::MakeReplySubject(header.GetSubject()));
    }

    m_IsComposeHeader = false;
  }
  else if (m_State == StateForwardMessage)
  {
    curs_set(1);
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = L"";
    m_ComposeHeaderStr[1] = L"";
    m_ComposeHeaderStr[2] = L"";
    m_ComposeHeaderStr[3] = L"";
    m_ComposeHeaderLine = 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    std::map<uint32_t, Header> headers;
    std::map<uint32_t, Body> bodys;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      headers = m_Headers[m_CurrentFolder];
      bodys = m_Bodys[m_CurrentFolder];
    }

    std::map<uint32_t, Header>::const_iterator hit = headers.find(m_MessageListCurrentUid);
    std::map<uint32_t, Body>::const_iterator bit = bodys.find(m_MessageListCurrentUid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      const Header& header = hit->second;
      const Body& body = bit->second;

      m_ComposeMessageStr =
        Util::ToWString("\n\n---------- Forwarded message ---------\n"
                        "From: " + header.GetFrom() + "\n"
                        "Date: " + header.GetDate() + "\n"
                        "Subject: " + header.GetSubject() + "\n"
                        "To: " + header.GetTo() + "\n");
      if (!header.GetCc().empty())
      {
        m_ComposeMessageStr +=
          Util::ToWString("Cc: " + header.GetCc());
      }

      m_ComposeMessageStr += Util::ToWString("\n" + body.GetTextPlain() + "\n");
      m_ComposeHeaderStr[3] = Util::ToWString(Util::MakeForwardSubject(header.GetSubject()));
    }

    m_IsComposeHeader = true;
  }

}

void Ui::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  char drawRequest = DrawRequestNone;

  if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Folders = p_Response.m_Folders;
    drawRequest |= DrawRequestAll;
  }

  if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
  {
    LOG_DEBUG("uids for \"%s\" = %d", p_Response.m_Folder.c_str(), p_Response.m_Uids.size());
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Uids[p_Response.m_Folder] = p_Response.m_Uids;
    UpdateMsgList(p_Response.m_Folder);
    drawRequest |= DrawRequestAll;
  }

  if (!p_Request.m_GetHeaders.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed))
  {
    LOG_DEBUG("headers for \"%s\" = %d", p_Response.m_Folder.c_str(), p_Response.m_Headers.size());
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Headers[p_Response.m_Folder].insert(p_Response.m_Headers.begin(),
                                          p_Response.m_Headers.end());
    UpdateMsgList(p_Response.m_Folder);
    drawRequest |= DrawRequestAll;
  }

  if (!p_Request.m_GetFlags.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed))
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::map<uint32_t, uint32_t> newFlags = p_Response.m_Flags;
    newFlags.insert(m_Flags[p_Response.m_Folder].begin(), m_Flags[p_Response.m_Folder].end());
    m_Flags[p_Response.m_Folder] = newFlags;
    drawRequest |= DrawRequestAll;
  }

  if (!p_Request.m_GetBodys.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed))
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Bodys[p_Response.m_Folder].insert(p_Response.m_Bodys.begin(), p_Response.m_Bodys.end());
    drawRequest |= DrawRequestAll;
  }

  if (p_Response.m_ResponseStatus != ImapManager::ResponseStatusOk)
  {
    if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed)
    {
      SetDialogMessage("Get folders failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed)
    {
      SetDialogMessage("Get message body failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed)
    {
      SetDialogMessage("Get message headers failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed)
    {
      SetDialogMessage("Get message ids failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed)
    {
      SetDialogMessage("Get message flags failed");
    }
  }

  AsyncDrawRequest(drawRequest);
}

void Ui::ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    if (!p_Action.m_MoveDestination.empty())
    {
      SetDialogMessage("Move message failed");
    }
    else if (p_Action.m_SetSeen || p_Action.m_SetUnseen)
    {
      SetDialogMessage("Update message flags failed");
    }
  }
}

void Ui::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    SetDialogMessage("Send message failed");
  }
}

void Ui::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Status.Update(p_StatusUpdate);
  AsyncDrawRequest(DrawRequestAll);
}

void Ui::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_ImapManager = p_ImapManager;
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

void Ui::SetTrashFolder(const std::string &p_TrashFolder)
{
  m_TrashFolder = p_TrashFolder;
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
  return m_Status.ToString();
}

std::string Ui::GetStateStr()
{
  switch (m_State)
  {
    case StateViewMessageList: return "Folder: " + m_CurrentFolder;
    case StateViewMessage: return std::string("Message ") + (m_Plaintext ? "plain" : "html");
    case StateGotoFolder: return "Goto Folder";
    case StateMoveToFolder: return "Move To Folder";
    case StateComposeMessage: return "Compose";
    case StateReplyMessage: return "Reply";
    case StateForwardMessage: return "Forward";
    default: return "Unknown State";
  }
}

bool Ui::IsValidTextKey(int p_Key)
{
  return ((p_Key >= 0x20) || (p_Key == 0x9) || (p_Key == 0xA));
}

void Ui::SendComposedMessage()
{
  SmtpManager::Action action;
  action.m_To = Util::ToString(m_ComposeHeaderStr.at(0));
  action.m_Cc = Util::ToString(m_ComposeHeaderStr.at(1));
  action.m_Att = Util::ToString(m_ComposeHeaderStr.at(2));
  action.m_Subject = Util::ToString(m_ComposeHeaderStr.at(3));
  action.m_Body = Util::ToString(Util::Join(m_ComposeMessageLines));

  m_SmtpManager->AsyncAction(action);
}

bool Ui::DeleteMessage()
{
  if (!m_TrashFolder.empty())
  {
    ImapManager::Action action;
    action.m_Folder = m_CurrentFolder;
    action.m_Uids.insert(m_MessageListCurrentUid);
    action.m_MoveDestination = m_TrashFolder;
    m_ImapManager->AsyncAction(action);

    //m_HasRequestedUids[m_CurrentFolder] = false;
    m_HasRequestedUids[m_TrashFolder] = false;

    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Uids[m_CurrentFolder] = m_Uids[m_CurrentFolder] - action.m_Uids;
      m_Uids[m_TrashFolder] = m_Uids[m_TrashFolder] + action.m_Uids;
      m_Headers[m_CurrentFolder] = m_Headers[m_CurrentFolder] - action.m_Uids;
      UpdateMsgList(m_CurrentFolder);
      UpdateMsgList(m_TrashFolder);
    }

    UpdateCurrentUid();    

    std::vector<std::pair<uint32_t, Header>> msgList;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      msgList = m_MsgList[m_CurrentFolder];
    }

    if (msgList.empty())
    {
      SetState(StateViewMessageList);
    }
    
    return true;
  }
  else
  {
    SetDialogMessage("Trash folder not configured");
    return false;
  }
}

void Ui::ToggleUnseen()
{
  std::map<uint32_t, uint32_t> flags;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    flags = m_Flags[m_CurrentFolder];
  }
  uint32_t uid = m_MessageListCurrentUid;
  bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));
  bool newSeen = !oldSeen;

  ImapManager::Action action;
  action.m_Folder = m_CurrentFolder;
  action.m_Uids.insert(m_MessageListCurrentUid);
  action.m_SetSeen = newSeen;
  action.m_SetUnseen = !newSeen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Flag::SetSeen(m_Flags[m_CurrentFolder][uid], newSeen);
  }
}

void Ui::MarkSeen()
{
  std::map<uint32_t, uint32_t> flags;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    flags = m_Flags[m_CurrentFolder];
  }
  uint32_t uid = m_MessageListCurrentUid;
  bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));

  if (oldSeen) return;

  bool newSeen = true;

  ImapManager::Action action;
  action.m_Folder = m_CurrentFolder;
  action.m_Uids.insert(m_MessageListCurrentUid);
  action.m_SetSeen = newSeen;
  action.m_SetUnseen = !newSeen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Flag::SetSeen(m_Flags[m_CurrentFolder][uid], newSeen);
  }
}

void Ui::UpdateCurrentUid()
{
  std::vector<std::pair<uint32_t, Header>> msgList;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    msgList = m_MsgList[m_CurrentFolder];
  }
  
  m_MessageListCurrentIndex = Util::Bound(0, m_MessageListCurrentIndex, (int)msgList.size() - 1);
  if (msgList.size() > 0)
  {
    uint32_t uid = std::prev(msgList.end(), m_MessageListCurrentIndex + 1)->first;
    m_MessageListCurrentUid = uid;
  }
}

void Ui::UpdateMsgList(const std::string& p_Folder)
{
  // called with lock held
  std::set<uint32_t> uids;
  std::map<uint32_t, Header> headers;
  uids = m_Uids[p_Folder];
  headers = m_Headers[p_Folder];

  // remove all uids not present in headers
  for (auto it = uids.begin(); it != uids.end(); /* incremented in loop */)
  {
    it = (headers.find(*it) == headers.end()) ? uids.erase(it) : std::next(it);
  }

  // create list
  std::vector<std::pair<uint32_t, Header>> msgList;
  for (auto it = uids.begin(); it != uids.end(); ++it)
  {
    msgList.push_back(std::make_pair(*it, headers.at(*it)));
  }

  // sort based on date
  std::sort(msgList.begin(), msgList.end(),
            [](const std::pair<uint32_t, Header>& m1, const std::pair<uint32_t, Header>& m2)
            {
              return m1.second.GetDate() < m2.second.GetDate();
            });
  
  m_MsgList[p_Folder] = msgList;
}

void Ui::ComposeMessagePrevLine()
{
  if (m_ComposeMessageWrapLine > 0)
  {
    if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() >
        m_ComposeMessageWrapPos)
    {
      int stepsBack = m_ComposeMessageWrapPos + 1 +
        (m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() -
         m_ComposeMessageWrapPos);
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - stepsBack,
                                        (int)m_ComposeMessageStr.size());
    }
    else
    {
      int stepsBack = m_ComposeMessageWrapPos + 1;
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - stepsBack,
                                        (int)m_ComposeMessageStr.size());
    }
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
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
  else if ((int)m_ComposeMessageLines.size() > 0)
  {
    int stepsForward = m_ComposeMessageLines[m_ComposeMessageWrapLine].size() -
      m_ComposeMessageWrapPos;
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
}
