// textstyle.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

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
  };

  struct MarkdownAttrs
  {
    int m_Bold = 0;
    int m_Italic = 0;
    int m_Code = 0;
    int m_Strike = 0;
  };

  typedef std::vector<std::vector<Span>> LineSpans;

  // style header field names (optionally padded) in lines before p_End, any names for raw header
  static void StyleHeaderLines(const std::vector<std::wstring>& p_Lines, size_t p_End, int p_Attrs,
                               bool p_RawHeader, LineSpans& p_Spans);

  // style markdown from p_Begin, markup is removed from p_Lines (incl. code fence lines)
  static void StyleMarkdownLines(std::vector<std::wstring>& p_Lines, size_t p_Begin,
                                 const MarkdownAttrs& p_Attrs, LineSpans& p_Spans);

  // style signature from first unquoted separator line (-- or em dash) after p_Begin to end
  static void StyleSignatureLines(const std::vector<std::wstring>& p_Lines, size_t p_Begin, int p_Attrs,
                                  LineSpans& p_Spans);

  // or attrs, with overlay color pair replacing base color pair
  static int CombineAttrs(int p_Base, int p_Over);

private:
  static void StyleParagraph(std::vector<std::wstring>& p_Lines, size_t p_Begin, size_t p_End,
                             int p_BaseAttrs, const MarkdownAttrs& p_Attrs, LineSpans& p_Spans,
                             bool p_IsTable);
  static void SplitQuotePrefix(const std::wstring& p_Line, std::wstring& p_Prefix, std::wstring& p_Content);
};
