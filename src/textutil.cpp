// textutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "textutil.h"

#include <algorithm>
#include <regex>

#include "strutil.h"
#include "textwrap.h"

std::string TextUtil::ReduceIndent(const std::string& p_Str, int p_Cnt)
{
  std::string tmpstr = "\n" + p_Str;
  std::string findstr = "\n ";
  std::string replacestr = "\n";
  for (int i = 0; i < p_Cnt; ++i)
  {
    StrUtil::ReplaceString(tmpstr, findstr, replacestr);
  }

  return tmpstr.substr(1);
}

std::string TextUtil::AddIndent(const std::string& p_Str, const std::string& p_Indent)
{
  std::string tmpstr = "\n" + p_Str;
  std::string findstr = "\n";
  std::string replacestr = "\n" + p_Indent;
  StrUtil::ReplaceString(tmpstr, findstr, replacestr);

  return tmpstr.substr(1);
}

std::vector<std::wstring> TextUtil::WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                             bool p_ProcessFormatFlowed, bool p_OutputFormatFlowed,
                                             bool p_QuoteWrap, int p_ExpandTabSize)
{
  return TextWrap::WordWrap(p_Text, p_LineLength, p_ProcessFormatFlowed, p_OutputFormatFlowed,
                            p_QuoteWrap, p_ExpandTabSize);
}

std::vector<std::wstring> TextUtil::WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                             bool p_ProcessFormatFlowed, bool p_OutputFormatFlowed,
                                             bool p_QuoteWrap, int p_ExpandTabSize,
                                             int p_Pos, int& p_WrapLine, int& p_WrapPos,
                                             std::vector<int>* p_LineOffsets /*= nullptr*/)
{
  return TextWrap::WordWrap(p_Text, p_LineLength, p_ProcessFormatFlowed, p_OutputFormatFlowed,
                            p_QuoteWrap, p_ExpandTabSize, p_Pos, p_WrapLine, p_WrapPos, p_LineOffsets);
}

bool TextUtil::GetQuotePrefix(const std::string& p_String, std::string& p_Prefix, std::string& p_Line)
{
  std::smatch sm;
  std::regex re("^(( *> *)+)");
  if (std::regex_search(p_String, sm, re))
  {
    p_Prefix = sm.str();
    p_Line = sm.suffix();
    return true;
  }
  else
  {
    p_Prefix.clear();
    p_Line = p_String;
    return false;
  }
}

bool TextUtil::GetQuotePrefix(const std::wstring& p_String, std::wstring& p_Prefix, std::wstring& p_Line)
{
  return TextWrap::GetQuotePrefix(p_String, p_Prefix, p_Line);
}

void TextUtil::DeleteToNextMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars)
{
  int searchPos = std::max(0, (p_Pos + p_Offs));
  size_t nextMatchPos = p_Str.find_first_of(p_Chars, searchPos);
  if (nextMatchPos != std::string::npos)
  {
    p_Str.erase(p_Pos, (nextMatchPos - searchPos + 1));
  }
  else
  {
    p_Str.erase(p_Pos);
  }

  p_Pos = std::min(p_Pos, (int)p_Str.size());
}

void TextUtil::DeleteToPrevMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars)
{
  int searchPos = std::max(0, (p_Pos + p_Offs));
  size_t prevMatchPos = p_Str.find_last_of(p_Chars, searchPos);
  if (prevMatchPos == std::string::npos)
  {
    prevMatchPos = 0;
  }

  p_Str.erase(prevMatchPos, p_Pos - prevMatchPos);
  p_Pos = std::min((int)prevMatchPos, (int)p_Str.size());
}

void TextUtil::JumpToNextMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars)
{
  int searchPos = std::max(0, (p_Pos + p_Offs));
  size_t nextMatchPos = p_Str.find_first_of(p_Chars, searchPos);
  if (nextMatchPos != std::string::npos)
  {
    p_Pos = std::min((int)nextMatchPos, (int)p_Str.size());
  }
  else
  {
    p_Pos = p_Str.size();
  }
}

void TextUtil::JumpToPrevMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars)
{
  int searchPos = std::max(0, (p_Pos + p_Offs));
  size_t prevMatchPos = p_Str.find_last_of(p_Chars, searchPos);
  if (prevMatchPos != std::string::npos)
  {
    p_Pos = std::min((int)prevMatchPos + 1, (int)p_Str.size());
  }
  else
  {
    p_Pos = 0;
  }
}

std::string TextUtil::UnwrapQuotedLines(const std::string& p_Msg, int p_LineLen)
{
  const std::vector<std::string> lines = StrUtil::Split(StrUtil::Strip(p_Msg, '\r'), '\n');
  const int lineCnt = lines.size();
  std::vector<std::string> outlines;
  std::string outline;
  std::string outprefix;
  std::string curline;
  std::string curprefix;
  for (int i = 0; i < lineCnt; ++i)
  {
    GetQuotePrefix(lines[i], curprefix, curline);
    const std::string curtrimline = StrUtil::Trim(curline);
    const int curfirstwordlen = curtrimline.empty() ? 0 : StrUtil::Split(curtrimline, ' ').at(0).size();
    const int prevlinesize = (i > 0) ? lines[i - 1].size() : 0;
    if ((outprefix == curprefix) && !curtrimline.empty() && ((prevlinesize + curfirstwordlen + 1) >= p_LineLen))
    {
      // Same prefix and previous line plus current lines first word exceed wrap width -> unwrap and append to outline
      if (!outline.empty())
      {
        outline += " ";
      }

      outline += curline;
    }
    else
    {
      if (!outline.empty())
      {
        // Terminate non-empty outline and start new
        outlines.push_back(outprefix + outline);
        outline = "";
      }

      if (curtrimline.empty())
      {
        // Append blank line for empty input line
        outlines.push_back(curprefix + curtrimline);
        outline = "";
      }
      else
      {
        // Set outline to current line
        outline = curtrimline;
      }

      // Update outprefix
      outprefix = curprefix;
    }
  }

  if (!outline.empty())
  {
    // Append any remaining buffered outline
    outlines.push_back(outprefix + outline);
  }

  // Return joined string
  return StrUtil::Join(outlines, "\n");
}
