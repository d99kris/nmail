// textstyle.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>
#include <vector>

class TextStyle
{
public:
  struct Span
  {
    int m_Start = 0;
    int m_Len = 0;
    int m_Attrs = 0;
    int m_Link = -1; // link index, if part of link
  };

  struct MarkdownAttrs
  {
    int m_Bold = 0;
    int m_Italic = 0;
    int m_Code = 0;
    int m_Strike = 0;
    int m_Link = 0;
  };

  struct Link
  {
    std::wstring m_Url;
    size_t m_Line = 0; // first line
  };

  typedef std::vector<std::vector<Span>> LineSpans;

  // style header field names (optionally padded) in lines before p_End, any names for raw header
  static void StyleHeaderLines(const std::vector<std::wstring>& p_Lines, size_t p_End, int p_Attrs,
                               bool p_RawHeader, LineSpans& p_Spans);

  // style markdown from p_Begin, markup is removed from p_Lines (incl. code fence lines), links
  // are parsed when p_Links is set, and their reference definitions at end removed
  static void StyleMarkdownLines(std::vector<std::wstring>& p_Lines, size_t p_Begin,
                                 const MarkdownAttrs& p_Attrs, LineSpans& p_Spans,
                                 std::vector<Link>* p_Links);

  // style signature from first unquoted separator line (-- or em dash) after p_Begin to end
  static void StyleSignatureLines(const std::vector<std::wstring>& p_Lines, size_t p_Begin, int p_Attrs,
                                  LineSpans& p_Spans);

  // or attrs, with overlay color pair replacing base color pair
  static int CombineAttrs(int p_Base, int p_Over);

private:
  typedef std::map<int, std::wstring> Refs;

  static void StyleParagraph(std::vector<std::wstring>& p_Lines, size_t p_Begin, size_t p_End,
                             int p_BaseAttrs, const MarkdownAttrs& p_Attrs, LineSpans& p_Spans,
                             bool p_IsTable, const Refs& p_Refs, std::vector<Link>* p_Links);
  static size_t ParseRefs(const std::vector<std::wstring>& p_Lines, size_t p_Begin, size_t& p_End,
                          Refs& p_Refs);
  static bool IsWebUrl(const std::wstring& p_Url);
  static int ParseRefNum(const std::wstring& p_Text, size_t p_Pos, size_t& p_End);
  static void SplitQuotePrefix(const std::wstring& p_Line, std::wstring& p_Prefix, std::wstring& p_Content);
};
