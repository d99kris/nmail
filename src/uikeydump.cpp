// uikeydump.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uikeydump.h"

#include <string>

#include <unistd.h>

#include <sys/select.h>

#include <ncurses.h>

#include "apputil.h"
#include "uikeyconfig.h"
#include "uikeyinput.h"

void UiKeyDump::Run()
{
  UiKeyInput::EnableMetaSendsEsc();
  AppUtil::InitLocale();
  initscr();
  noecho();
  cbreak();
  raw();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(0);

  printw("key code dump mode - press ctrl-c or 'q' to exit\n");
  refresh();

  UiKeyConfig::Init(false);

  bool running = true;
  while (running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = { 1, 0 };
    select(maxfd + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      int y = 0;
      int x = 0;
      int maxy = 0;
      int maxx = 0;
      getyx(stdscr, y, x);
      getmaxyx(stdscr, maxy, maxx);
      if (y == (maxy - 1))
      {
        clear();
        refresh();
      }

      int count = 0;
      wint_t key = 0;
      wint_t keyOk = 0;
      while (UiKeyInput::GetWch(&key) != ERR)
      {
        keyOk = key;
        ++count;
        printw("\\%o", key);

        if ((key == 3) || (key == 'q'))
        {
          running = false;
          break;
        }
      }

      if ((keyOk != 0) && (count == 1))
      {
        std::string keyName = UiKeyConfig::GetKeyName(keyOk);
        if (!keyName.empty())
        {
          printw(" %s", keyName.c_str());
        }

        // output terminal key code sequence, which unlike the key code above
        // is not dependent on terminfo, and thus preferred for use in key.conf
        std::string keySequence = UiKeyConfig::GetKeySequence(keyOk);
        if (!keySequence.empty())
        {
          printw(" (%s)", keySequence.c_str());
        }
      }

      printw("\n");
      refresh();
    }
  }

  UiKeyConfig::Cleanup();
  wclear(stdscr);
  endwin();
  UiKeyInput::RestoreMetaSendsEsc();
}
