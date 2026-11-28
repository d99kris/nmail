// uidialogview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uidialogview.h"

#include <algorithm>

#include <unistd.h>

#include "apputil.h"
#include "loghelp.h"
#include "strutil.h"
#include "textwrap.h"
#include "uiconfig.h"
#include "uihelpview.h"
#include "uikeyinput.h"

UiDialogView::UiDialogView(Ui* p_Ui)
  : UiViewBase(p_Ui)
{
  m_HelpEnabled = UiConfig::GetBool("help_enabled");
}

void UiDialogView::Draw()
{
  werase(m_DialogWin);

  {
    std::lock_guard<std::mutex> lock(m_DialogMutex);
    std::chrono::time_point<std::chrono::system_clock> nowTime =
      std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = nowTime - m_DialogMessageTime;
    if ((elapsed.count() < 0.5f) && !m_DialogMessage.empty())
    {
      std::wstring wmsg = StrUtil::ToWString(m_DialogMessage);
      const int maxWidth = std::max(m_ScreenWidth - 2, 0);
      if ((int)wmsg.size() > maxWidth)
      {
        static const std::wstring suffix = L"...";
        wmsg = (maxWidth > (int)suffix.size())
          ? (wmsg.substr(0, maxWidth - suffix.size()) + suffix)
          : wmsg.substr(0, maxWidth);
      }

      const std::string dispStr = StrUtil::ToString(wmsg);
      int x = std::max((m_ScreenWidth - (int)wmsg.size() - 1) / 2, 0);
      wattron(m_DialogWin, m_AttrsDialog);
      mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
      wattroff(m_DialogWin, m_AttrsDialog);
    }
  }

  RefreshWin(m_DialogWin);
}

void UiDialogView::SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn /*= false */)
{
  std::lock_guard<std::mutex> lock(m_DialogMutex);
  m_DialogMessage = p_DialogMessage;
  m_DialogMessageTime = std::chrono::system_clock::now();
  if (!p_DialogMessage.empty())
  {
    const std::string& logMessage = StrUtil::ToLower(p_DialogMessage);
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

int UiDialogView::ReadKeyBlocking()
{
  while (true)
  {
    if (AppUtil::IsTerminateRequested()) return m_KeyCancel;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = { 1, 0 };
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      UiKeyInput::GetWch(&key);

      return key;
    }
  }
}

bool UiDialogView::PromptYesNo(const std::string& p_Prompt)
{
  werase(m_DialogWin);

  const std::string& dispStr = p_Prompt;
  int x = std::max((m_ScreenWidth - (int)dispStr.size() - 1) / 2, 0);
  wattron(m_DialogWin, m_AttrsDialog);
  mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
  wattroff(m_DialogWin, m_AttrsDialog);

  wrefresh(m_DialogWin);

  int key = ReadKeyBlocking();

  return ((key == 'y') || (key == 'Y'));
}

bool UiDialogView::PromptString(const std::string& p_Prompt, const std::string& p_Action,
                                std::string& p_Entry)
{
  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    const std::vector<std::vector<std::string>> promptHelp =
    {
      {
        m_HelpView->GetKeyDisplay(m_KeyReturn), p_Action,
      },
      {
        m_HelpView->GetKeyDisplay(m_KeyCancel), "Cancel",
      }
    };

    m_HelpView->DrawHelpText(promptHelp, 2);

    wrefresh(m_HelpWin);
  }

  curs_set(1);

  m_FilenameEntryString = StrUtil::ToWString(p_Entry);
  m_FilenameEntryStringPos = m_FilenameEntryString.size();

  bool rv = false;
  while (true)
  {
    werase(m_DialogWin);

    const std::string& dispStr = p_Prompt + StrUtil::ToString(m_FilenameEntryString);
    mvwprintw(m_DialogWin, 0, 3, "%s", dispStr.c_str());

    leaveok(m_DialogWin, false);
    const int promptWidth = TextWrap::GetWidth(StrUtil::ToWString(p_Prompt));
    const int entryWidth = TextWrap::GetWidth(m_FilenameEntryString.substr(0, m_FilenameEntryStringPos));
    wmove(m_DialogWin, 0, 3 + promptWidth + entryWidth);
    wrefresh(m_DialogWin);
    leaveok(m_DialogWin, true);

    int key = ReadKeyBlocking();
    if (key == m_KeyCancel)
    {
      rv = false;
      break;
    }
    else if ((key == m_KeyReturn) || (key == m_KeyEnter))
    {
      p_Entry = StrUtil::ToString(m_FilenameEntryString);
      rv = true;
      break;
    }
    else if ((key == m_KeyUp) || (key == m_KeyDown) ||
             (key == m_KeyPrevPageCompose) || (key == m_KeyNextPageCompose) ||
             (key == m_KeyHome) || (key == m_KeyEnd))
    {
      // ignore
    }
    else if (HandleLineKey(key, m_FilenameEntryString, m_FilenameEntryStringPos))
    {
      // none
    }
    else if (HandleTextKey(key, m_FilenameEntryString, m_FilenameEntryStringPos))
    {
      // none
    }
  }

  curs_set(0);
  return rv;
}
