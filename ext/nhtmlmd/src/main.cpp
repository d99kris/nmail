// main.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "nhtmlmd.h"

static void ShowHelp();
static void ShowVersion();

int main(int argc, char* argv[])
{
  Nhtmlmd::HtmlOptions htmlOptions;
  Nhtmlmd::MarkdownOptions markdownOptions;
  bool md2html = false;
  bool text = false;
  std::string path;

  // Argument handling
  std::vector<std::string> args(argv + 1, argv + argc);
  for (auto it = args.begin(); it != args.end(); ++it)
  {
    if ((*it == "-b") || (*it == "--hard-breaks"))
    {
      markdownOptions.m_HardBreaks = true;
    }
    else if ((*it == "-h") || (*it == "--help"))
    {
      ShowHelp();
      return 0;
    }
    else if (((*it == "-l") || (*it == "--links")) && (std::distance(it + 1, args.end()) > 0))
    {
      ++it;
      if (*it == "text")
      {
        htmlOptions.m_LinkStyle = Nhtmlmd::LinkStyleText;
      }
      else if (*it == "refs")
      {
        htmlOptions.m_LinkStyle = Nhtmlmd::LinkStyleRefs;
      }
      else if (*it == "inline")
      {
        htmlOptions.m_LinkStyle = Nhtmlmd::LinkStyleInline;
      }
      else
      {
        ShowHelp();
        return 1;
      }
    }
    else if ((*it == "-m") || (*it == "--md2html"))
    {
      md2html = true;
    }
    else if ((*it == "-t") || (*it == "--text"))
    {
      text = true;
    }
    else if ((*it == "-v") || (*it == "--version"))
    {
      ShowVersion();
      return 0;
    }
    else if (((*it == "-w") || (*it == "--width")) && (std::distance(it + 1, args.end()) > 0))
    {
      ++it;
      htmlOptions.m_LayoutWidth = std::max(1, atoi(it->c_str()));
    }
    else if (path.empty() && ((*it == "-") || (it->compare(0, 1, "-") != 0)))
    {
      path = *it;
    }
    else
    {
      ShowHelp();
      return 1;
    }
  }

  // Read input
  std::string input;
  if (path.empty() || (path == "-"))
  {
    input.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
  }
  else
  {
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
      std::cerr << "nhtmlmd: cannot read " << path << "\n";
      return 1;
    }

    std::stringstream sstream;
    sstream << file.rdbuf();
    input = sstream.str();
  }

  // Convert
  if (md2html)
  {
    std::cout << Nhtmlmd::MarkdownToHtml(input, markdownOptions);
  }
  else if (text)
  {
    std::cout << Nhtmlmd::HtmlToText(input, htmlOptions);
  }
  else
  {
    std::cout << Nhtmlmd::HtmlToMarkdown(input, htmlOptions);
  }

  return 0;
}

static void ShowHelp()
{
  std::cout <<
    "nhtmlmd converts html to markdown or plain text, and markdown to html. It is\n"
    "tuned for email: hidden preheaders, layout tables, tracking images and quotes\n"
    "are handled to give readable text.\n"
    "\n"
    "Usage: nhtmlmd [OPTION]... [FILE]\n"
    "\n"
    "Reads FILE, or stdin if FILE is omitted or -, and writes the result to stdout.\n"
    "Input shall be utf-8.\n"
    "\n"
    "Options:\n"
    "   -b,  --hard-breaks         treat every newline as a line break (with -m)\n"
    "   -h,  --help                display this help and exit\n"
    "   -l,  --links <STYLE>       link style for html input: text (default), refs\n"
    "                              (numbered, listed at end) or inline\n"
    "   -m,  --md2html             convert markdown to html\n"
    "   -t,  --text                convert html to plain text instead of markdown\n"
    "   -v,  --version             output version information and exit\n"
    "   -w,  --width <N>           max width of laid out content such as tables\n"
    "                              (default 80)\n"
    "\n"
    "Examples:\n"
    "   nhtmlmd mail.html          convert html to markdown, links as text\n"
    "   nhtmlmd -t mail.html       convert html to plain text\n"
    "   nhtmlmd -l refs mail.html  convert html to markdown with numbered links\n"
    "   nhtmlmd -m -b reply.md     convert markdown to html as nmail sends it\n"
    "\n";
}

static void ShowVersion()
{
  std::cout <<
    "nhtmlmd v" << Nhtmlmd::GetVersion() << "\n"
    "\n"
    "Copyright (c) 2026 Kristofer Berggren\n"
    "\n"
    "nhtmlmd is distributed under the MIT license.\n"
    "\n"
    "Written by Kristofer Berggren.\n";
}
