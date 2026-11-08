// textstyle.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "textstyle.h"

#include <algorithm>
#include <cwctype>

#include <ncurses.h>

void TextStyle::StyleHeaderLines(const std::vector<std::wstring>& p_Lines, size_t p_End, int p_Attrs,
                                 bool p_RawHeader, LineSpans& p_Spans)
{
  static const std::vector<std::wstring> names =
  {
    L"Date:", L"From:", L"Reply-To:", L"To:", L"Cc:", L"Bcc:", L"Subject:", L"Attachments:",
  };

  for (size_t i = 0; (i < p_End) && (i < p_Lines.size()); ++i)
  {
    const std::wstring& line = p_Lines.at(i);
    if (p_RawHeader)
    {
      // field name: letter, then letters, digits or dash, then colon and space
      if (line.empty() || !iswalpha(line[0])) continue;

      size_t pos = 1;
      while ((pos < line.size()) && (iswalnum(line[pos]) || (line[pos] == L'-')))
      {
        ++pos;
      }

      if ((pos < line.size()) && (line[pos] == L':') && (((pos + 1) == line.size()) || (line[pos + 1] == L' ')))
      {
        p_Spans[i].push_back(Span{ 0, (int)(pos + 1), p_Attrs });
      }

      continue;
    }

    for (const auto& name : names)
    {
      if (line.rfind(name, 0) == 0)
      {
        p_Spans[i].push_back(Span{ 0, (int)name.size(), p_Attrs });
        break;
      }
    }
  }
}

void TextStyle::StyleMarkdownLines(std::vector<std::wstring>& p_Lines, size_t p_Begin,
                                   const MarkdownAttrs& p_Attrs, LineSpans& p_Spans)
{
  auto isFence = [](const std::wstring& p_Content)
  {
    return (p_Content.rfind(L"```", 0) == 0);
  };

  auto isTable = [](const std::wstring& p_Content)
  {
    return (p_Content.rfind(L"|", 0) == 0);
  };

  auto isHeading = [](const std::wstring& p_Content)
  {
    const size_t level = p_Content.find_first_not_of(L'#');
    return (level >= 1) && (level <= 6) && (level < p_Content.size()) && (p_Content[level] == L' ');
  };

  bool inFence = false;
  std::vector<size_t> fenceLines;
  size_t i = p_Begin;
  while (i < p_Lines.size())
  {
    std::wstring prefix;
    std::wstring content;
    SplitQuotePrefix(p_Lines.at(i), prefix, content);

    // code block fences removed, lines styled as-is
    if (isFence(content))
    {
      inFence = !inFence;
      fenceLines.push_back(i);
      ++i;
      continue;
    }

    if (inFence)
    {
      if (!content.empty() && (p_Attrs.m_Code != A_NORMAL))
      {
        p_Spans[i].push_back(Span{ (int)prefix.size(), (int)content.size(), p_Attrs.m_Code });
      }

      ++i;
      continue;
    }

    if (content.empty())
    {
      ++i;
      continue;
    }

    // table rows styled one by one, with cells padded to keep alignment
    if (isTable(content))
    {
      StyleParagraph(p_Lines, i, i + 1, A_NORMAL, p_Attrs, p_Spans, true);
      ++i;
      continue;
    }

    // paragraph spanning wrapped lines with same quote prefix
    size_t end = i + 1;
    while (end < p_Lines.size())
    {
      std::wstring nextPrefix;
      std::wstring nextContent;
      SplitQuotePrefix(p_Lines.at(end), nextPrefix, nextContent);
      if ((nextPrefix != prefix) || nextContent.empty() || isFence(nextContent) || isTable(nextContent)) break;

      ++end;
    }

    StyleParagraph(p_Lines, i, end, isHeading(content) ? p_Attrs.m_Bold : A_NORMAL, p_Attrs, p_Spans, false);
    i = end;
  }

  for (auto it = fenceLines.rbegin(); it != fenceLines.rend(); ++it)
  {
    p_Lines.erase(p_Lines.begin() + *it);
    p_Spans.erase(p_Spans.begin() + *it);
  }
}

void TextStyle::StyleSignatureLines(const std::vector<std::wstring>& p_Lines, size_t p_Begin, int p_Attrs,
                                    LineSpans& p_Spans)
{
  // separator with trailing space often stripped, so also accept without, and
  // em dash used by notification emails (github, gitlab)
  size_t begin = p_Lines.size();
  for (size_t i = p_Begin; i < p_Lines.size(); ++i)
  {
    const std::wstring& line = p_Lines.at(i);
    if ((line == L"-- ") || (line == L"--") || (line == L"\u2014 ") || (line == L"\u2014"))
    {
      begin = i;
      break;
    }
  }

  for (size_t i = begin; i < p_Lines.size(); ++i)
  {
    const std::wstring& line = p_Lines.at(i);
    if (line.empty() || (line[0] == L'>')) continue;

    // insert first, so markdown styles are applied on top
    p_Spans[i].insert(p_Spans[i].begin(), Span{ 0, (int)line.size(), p_Attrs });
  }
}

int TextStyle::CombineAttrs(int p_Base, int p_Over)
{
  if (p_Over & A_COLOR) return (p_Base & ~A_COLOR) | p_Over;

  return p_Base | p_Over;
}

void TextStyle::StyleParagraph(std::vector<std::wstring>& p_Lines, size_t p_Begin, size_t p_End,
                               int p_BaseAttrs, const MarkdownAttrs& p_Attrs, LineSpans& p_Spans,
                               bool p_IsTable)
{
  // join paragraph content, with newline separating wrapped lines
  std::wstring text;
  std::vector<std::wstring> prefixes;
  for (size_t i = p_Begin; i < p_End; ++i)
  {
    std::wstring prefix;
    std::wstring content;
    SplitQuotePrefix(p_Lines.at(i), prefix, content);
    if (i > p_Begin) text += L'\n';

    text += content;
    prefixes.push_back(prefix);
  }

  const size_t len = text.size();
  std::vector<int> attrs(len, p_BaseAttrs);
  std::vector<bool> hidden(len, false);
  std::vector<bool> code(len, false);

  // code spans
  for (size_t pos = 0; pos < len; ++pos)
  {
    if (text[pos] != L'`') continue;

    const size_t close = text.find(L'`', pos + 1);
    if (close == std::wstring::npos) break;

    if (close > (pos + 1))
    {
      for (size_t k = pos; k <= close; ++k)
      {
        code[k] = true;
        attrs[k] = CombineAttrs(attrs[k], p_Attrs.m_Code);
      }

      hidden[pos] = true;
      hidden[close] = true;
    }

    pos = close;
  }

  // escaped table cell separators
  if (p_IsTable)
  {
    for (size_t pos = 0; (pos + 1) < len; ++pos)
    {
      if ((text[pos] == L'\\') && (text[pos + 1] == L'|'))
      {
        hidden[pos] = true;
      }
    }
  }

  auto isSpace = [&](size_t p_Pos)
  {
    return (p_Pos >= len) || iswspace(text[p_Pos]);
  };

  auto isMarker = [&](size_t p_Pos, wchar_t p_Mark, size_t p_MarkLen)
  {
    if ((p_Pos + p_MarkLen) > len) return false;

    for (size_t k = p_Pos; k < (p_Pos + p_MarkLen); ++k)
    {
      if ((text[k] != p_Mark) || code[k] || hidden[k]) return false;
    }

    // single mark must not be part of double
    if (p_MarkLen == 1)
    {
      if ((p_Pos > 0) && (text[p_Pos - 1] == p_Mark) && !hidden[p_Pos - 1]) return false;

      if (((p_Pos + 1) < len) && (text[p_Pos + 1] == p_Mark)) return false;
    }

    return true;
  };

  // closer search bounded, and not repeated where it found none, to keep long
  // paragraphs with unmatched markers from taking quadratic time
  static const size_t maxEmphasisLen = 2000;
  size_t noCloseTo[3] = { 0, 0, 0 }; // per marker kind: *, ** and ~~

  // emphasis and strikethrough, opener followed and closer preceded by non-space
  for (size_t pos = 0; pos < len; ++pos)
  {
    wchar_t mark = L'*';
    size_t markLen = 0;
    if (isMarker(pos, L'*', 2))
    {
      markLen = 2;
    }
    else if (isMarker(pos, L'*', 1))
    {
      markLen = 1;
    }
    else if (isMarker(pos, L'~', 2))
    {
      mark = L'~';
      markLen = 2;
    }

    if (markLen == 0) continue;

    // avoid intra-word italic like a*b*c
    const bool wordBefore = (pos > 0) && iswalnum(text[pos - 1]);
    if (isSpace(pos + markLen) || ((markLen == 1) && wordBefore))
    {
      pos += markLen - 1;
      continue;
    }

    const size_t kind = (mark == L'~') ? 2 : (markLen - 1);
    size_t close = std::wstring::npos;
    const size_t end = std::min(len, pos + markLen + maxEmphasisLen);
    for (size_t k = std::max(pos + markLen + 1, noCloseTo[kind]); k < end; ++k)
    {
      const bool wordAfter = ((k + markLen) < len) && iswalnum(text[k + markLen]);
      if (isMarker(k, mark, markLen) && !isSpace(k - 1) && !((markLen == 1) && wordAfter))
      {
        close = k;
        break;
      }
    }

    if (close == std::wstring::npos)
    {
      noCloseTo[kind] = std::max(noCloseTo[kind], end);
    }

    // reject runs of markers, like ***** separators
    bool onlyMarks = (close != std::wstring::npos);
    for (size_t k = pos + markLen; onlyMarks && (k < close); ++k)
    {
      onlyMarks = (text[k] == mark);
    }

    if ((close == std::wstring::npos) || onlyMarks)
    {
      pos += markLen - 1;
      continue;
    }

    const int emphAttrs = (mark == L'~') ? p_Attrs.m_Strike : ((markLen == 2) ? p_Attrs.m_Bold : p_Attrs.m_Italic);
    for (size_t k = pos + markLen; k < close; ++k)
    {
      attrs[k] = CombineAttrs(attrs[k], emphAttrs);
    }

    for (size_t k = 0; k < markLen; ++k)
    {
      hidden[pos + k] = true;
      hidden[close + k] = true;
    }

    pos += markLen - 1;
  }

  // rebuild lines without hidden markers, and merge attrs into spans
  size_t pos = 0;
  for (size_t i = p_Begin; i < p_End; ++i)
  {
    std::wstring line = prefixes.at(i - p_Begin);
    std::vector<Span>& spans = p_Spans[i];
    size_t pad = 0;
    for (; (pos < len) && (text[pos] != L'\n'); ++pos)
    {
      if (hidden[pos])
      {
        ++pad;
        continue;
      }

      // pad table cell by hidden marker width before its separator
      if (p_IsTable && (text[pos] == L'|') && ((pos == 0) || (text[pos - 1] != L'\\')))
      {
        line += std::wstring(pad, L' ');
        pad = 0;
      }

      const int col = line.size();
      line += text[pos];
      if (attrs[pos] == A_NORMAL) continue;

      if (!spans.empty() && (spans.back().m_Attrs == attrs[pos]) &&
          ((spans.back().m_Start + spans.back().m_Len) == col))
      {
        ++spans.back().m_Len;
      }
      else
      {
        spans.push_back(Span{ col, 1, attrs[pos] });
      }
    }

    ++pos; // skip newline
    p_Lines[i] = line;
  }
}

// same as quote regex ^(( *> *)+) used by word wrap
void TextStyle::SplitQuotePrefix(const std::wstring& p_Line, std::wstring& p_Prefix, std::wstring& p_Content)
{
  const size_t prefixEnd = std::min(p_Line.find_first_not_of(L" >"), p_Line.size());
  const bool hasQuote = (p_Line.find(L'>') < prefixEnd);
  const size_t prefixLen = hasQuote ? prefixEnd : 0;
  p_Prefix = p_Line.substr(0, prefixLen);
  p_Content = p_Line.substr(prefixLen);
}
