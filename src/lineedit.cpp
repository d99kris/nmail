// lineedit.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "lineedit.h"

#include <cerrno>
#include <clocale>
#include <iostream>

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include "strutil.h"

#define KEY_CTRLA 1
#define KEY_CTRLB 2
#define KEY_CTRLC 3
#define KEY_CTRLD 4
#define KEY_CTRLE 5
#define KEY_CTRLF 6
#define KEY_CTRLH 8
#define KEY_CTRLK 11
#define KEY_CTRLU 21
#define KEY_CTRLW 23
#define KEY_ESC 27
#define KEY_DEL 127

bool LineEdit::Read(const std::string& p_Prompt, std::string& p_Value, bool p_ReplaceOnType)
{
  if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
  {
    return ReadEdit(p_Prompt, p_Value, p_ReplaceOnType);
  }

  return ReadPlain(p_Prompt, p_Value);
}

bool LineEdit::ReadPlain(const std::string& p_Prompt, std::string& p_Value)
{
  // non-interactive, empty input keeps current value
  std::cout << p_Prompt << std::flush;
  std::string line;
  if (!std::getline(std::cin, line))
  {
    std::cout << "\n";
    return false;
  }

  line = StrUtil::Trim(line);
  if (!line.empty())
  {
    p_Value = line;
  }

  return true;
}

bool LineEdit::ReadEdit(const std::string& p_Prompt, std::string& p_Value, bool p_ReplaceOnType)
{
  struct termios told;
  if (tcgetattr(STDIN_FILENO, &told) != 0) return ReadPlain(p_Prompt, p_Value);

  struct termios tnew = told;
  tnew.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
  tnew.c_iflag &= ~(IXON | ICRNL);
  tnew.c_cc[VMIN] = 1;
  tnew.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &tnew) != 0) return ReadPlain(p_Prompt, p_Value);

  // locale needed for character display width
  const std::string oldLocale = setlocale(LC_CTYPE, nullptr);
  setlocale(LC_CTYPE, "");

  std::wstring text = StrUtil::ToWString(p_Value);
  size_t cursor = text.size();
  bool done = false;
  bool cancel = false;
  bool replace = p_ReplaceOnType;
  Redraw(p_Prompt, text, cursor);

  while (!done && !cancel)
  {
    const int ch = ReadByte();
    switch (ch)
    {
      case -1:
      case KEY_CTRLC:
        cancel = true;
        break;

      case '\r':
      case '\n':
        done = true;
        break;

      case KEY_CTRLA:
        cursor = 0;
        break;

      case KEY_CTRLE:
        cursor = text.size();
        break;

      case KEY_CTRLB:
        if (cursor > 0) --cursor;
        break;

      case KEY_CTRLF:
        if (cursor < text.size()) ++cursor;
        break;

      case KEY_CTRLH:
      case KEY_DEL:
        if (cursor > 0)
        {
          text.erase(--cursor, 1);
        }
        break;

      case KEY_CTRLD:
        if (text.empty())
        {
          cancel = true;
        }
        else if (cursor < text.size())
        {
          text.erase(cursor, 1);
        }
        break;

      case KEY_CTRLK:
        text.erase(cursor);
        break;

      case KEY_CTRLU:
        text.erase(0, cursor);
        cursor = 0;
        break;

      case KEY_CTRLW:
        {
          size_t pos = cursor;
          while ((pos > 0) && (text[pos - 1] == L' ')) --pos;
          while ((pos > 0) && (text[pos - 1] != L' ')) --pos;
          text.erase(pos, cursor - pos);
          cursor = pos;
        }
        break;

      case KEY_ESC:
        {
          // escape sequence, e.g. ESC [ D or ESC O H, lone escape is ignored
          const int intro = ReadByte(50);
          if ((intro != '[') && (intro != 'O')) break;

          std::string seq;
          int seqCh = 0;
          while (((seqCh = ReadByte(50)) != -1) && (seq.size() < 8))
          {
            seq += (char)seqCh;
            if ((seqCh >= 0x40) && (seqCh <= 0x7e)) break;
          }

          if (seq == "D")
          {
            if (cursor > 0) --cursor;
          }
          else if (seq == "C")
          {
            if (cursor < text.size()) ++cursor;
          }
          else if ((seq == "H") || (seq == "1~") || (seq == "7~"))
          {
            cursor = 0;
          }
          else if ((seq == "F") || (seq == "4~") || (seq == "8~"))
          {
            cursor = text.size();
          }
          else if (seq == "3~")
          {
            if (cursor < text.size())
            {
              text.erase(cursor, 1);
            }
          }
        }
        break;

      default:
        if ((ch >= 0x20) && (ch != KEY_DEL))
        {
          // utf-8 sequence length from lead byte
          std::string bytes(1, (char)ch);
          const int extra = ((ch & 0xe0) == 0xc0) ? 1 : ((ch & 0xf0) == 0xe0) ? 2 : ((ch & 0xf8) == 0xf0) ? 3 : 0;
          for (int i = 0; i < extra; ++i)
          {
            const int contCh = ReadByte(50);
            if (contCh == -1) break;

            bytes += (char)contCh;
          }

          if (replace)
          {
            // typing replaces initial value
            text.clear();
            cursor = 0;
          }

          const std::wstring wstr = StrUtil::ToWString(bytes);
          text.insert(cursor, wstr);
          cursor += wstr.size();
        }
        break;
    }

    replace = false;
    if (!done && !cancel)
    {
      Redraw(p_Prompt, text, cursor);
    }
  }

  setlocale(LC_CTYPE, oldLocale.c_str());
  tcsetattr(STDIN_FILENO, TCSANOW, &told);
  std::cout << "\n" << std::flush;

  if (cancel) return false;

  p_Value = StrUtil::Trim(StrUtil::ToString(text));
  return true;
}

int LineEdit::ReadByte(int p_TimeoutMs)
{
  if (p_TimeoutMs >= 0)
  {
    struct pollfd fds = { STDIN_FILENO, POLLIN, 0 };
    if (poll(&fds, 1, p_TimeoutMs) <= 0) return -1;
  }

  unsigned char ch = 0;
  ssize_t rv = 0;
  do
  {
    // retry if interrupted by signal, e.g. terminal resize
    rv = read(STDIN_FILENO, &ch, 1);
  }
  while ((rv == -1) && (errno == EINTR));

  return (rv == 1) ? ch : -1;
}

void LineEdit::Redraw(const std::string& p_Prompt, const std::wstring& p_Text, size_t p_Cursor)
{
  std::string line = "\r" + p_Prompt + StrUtil::ToString(p_Text) + "\033[K";
  const int back = StrUtil::WStringWidth(p_Text.substr(p_Cursor));
  if (back > 0)
  {
    line += "\033[" + std::to_string(back) + "D";
  }

  std::cout << line << std::flush;
}
