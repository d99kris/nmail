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
    L"Date", L"From", L"Reply-To", L"To", L"Cc", L"Bcc", L"Subject", L"Attchmnt", L"Attachments",
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

    // known name, padded with spaces as in compose, then colon
    for (const auto& name : names)
    {
      if (line.rfind(name, 0) != 0) continue;

      const size_t colon = line.find_first_not_of(L' ', name.size());
      if ((colon != std::wstring::npos) && (line[colon] == L':'))
      {
        p_Spans[i].push_back(Span{ 0, (int)(colon + 1), p_Attrs });
        break;
      }
    }
  }
}

void TextStyle::StyleMarkdownLines(std::vector<std::wstring>& p_Lines, size_t p_Begin,
                                   const MarkdownAttrs& p_Attrs, LineSpans& p_Spans,
                                   std::vector<Link>* p_Links)
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

  // reference definitions at end, removed after styling
  Refs refs;
  size_t refsEnd = p_Lines.size();
  const size_t refsBegin = (p_Links != nullptr) ? ParseRefs(p_Lines, p_Begin, refsEnd, refs) : refsEnd;

  bool inFence = false;
  std::vector<bool> removeLines(p_Lines.size(), false);
  size_t i = p_Begin;
  while (i < refsBegin)
  {
    std::wstring prefix;
    std::wstring content;
    SplitQuotePrefix(p_Lines.at(i), prefix, content);

    // code block fences removed, lines styled as-is
    if (isFence(content))
    {
      inFence = !inFence;
      removeLines[i] = true;
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
      StyleParagraph(p_Lines, i, i + 1, A_NORMAL, p_Attrs, p_Spans, true, refs, p_Links);
      ++i;
      continue;
    }

    // paragraph spanning wrapped lines with same quote prefix
    size_t end = i + 1;
    while (end < refsBegin)
    {
      std::wstring nextPrefix;
      std::wstring nextContent;
      SplitQuotePrefix(p_Lines.at(end), nextPrefix, nextContent);
      if ((nextPrefix != prefix) || nextContent.empty() || isFence(nextContent) || isTable(nextContent)) break;

      ++end;
    }

    StyleParagraph(p_Lines, i, end, isHeading(content) ? p_Attrs.m_Bold : A_NORMAL, p_Attrs, p_Spans, false,
                   refs, p_Links);
    i = end;
  }

  for (size_t k = refsBegin; k < refsEnd; ++k)
  {
    removeLines[k] = true;
  }

  // remove lines in one pass
  size_t count = 0;
  for (size_t k = 0; k < p_Lines.size(); ++k)
  {
    if (removeLines[k]) continue;

    if (count != k)
    {
      p_Lines[count] = std::move(p_Lines[k]);
      p_Spans[count] = std::move(p_Spans[k]);
    }

    ++count;
  }

  p_Lines.resize(count);
  p_Spans.resize(count);

  if (p_Links == nullptr) return;

  std::vector<bool> found(p_Links->size(), false);
  for (size_t line = 0; line < p_Spans.size(); ++line)
  {
    for (const auto& span : p_Spans.at(line))
    {
      if ((span.m_Link < 0) || found.at(span.m_Link)) continue;

      found[span.m_Link] = true;
      p_Links->at(span.m_Link).m_Line = line;
    }
  }
}

void TextStyle::StyleUrlLines(const std::vector<std::wstring>& p_Lines, size_t p_Begin, int p_Attrs,
                              size_t p_LineLength, LineSpans& p_Spans, std::vector<Link>& p_Links)
{
  struct Segment
  {
    size_t m_Line = 0;
    size_t m_Start = 0;
    size_t m_End = 0;
  };

  size_t i = p_Begin;
  size_t pos = 0;
  while (i < p_Lines.size())
  {
    if (pos >= p_Lines.at(i).size())
    {
      ++i;
      pos = 0;
      continue;
    }

    const size_t schemeLen = ParseUrlScheme(p_Lines.at(i), pos);
    if (schemeLen == 0)
    {
      ++pos;
      continue;
    }

    // url chars, continued on next line (after same quote prefix) when wrapped at full width
    std::wstring url;
    std::vector<Segment> segments;
    size_t line = i;
    size_t start = pos;
    while (true)
    {
      const std::wstring& text = p_Lines.at(line);
      size_t end = start;
      while ((end < text.size()) && IsUrlChar(text[end]))
      {
        ++end;
      }

      url += text.substr(start, end - start);
      segments.push_back(Segment{ line, start, end });
      if ((end < text.size()) || (text.size() != p_LineLength) || ((line + 1) >= p_Lines.size())) break;

      std::wstring prefix;
      std::wstring content;
      SplitQuotePrefix(text, prefix, content);
      const std::wstring& next = p_Lines.at(line + 1);
      if ((next.rfind(prefix, 0) != 0) || (next.size() <= prefix.size()) ||
          !IsUrlChar(next[prefix.size()])) break;

      ++line;
      start = prefix.size();
    }

    // trailing punctuation, and closing brackets without opening bracket in url
    auto isTrailing = [&](wchar_t p_Ch)
    {
      static const std::wstring punct = L".,;:!?'*_";
      static const std::wstring closing = L")]}";
      static const std::wstring opening = L"([{";
      if (punct.find(p_Ch) != std::wstring::npos) return true;

      const size_t bracket = closing.find(p_Ch);
      if (bracket == std::wstring::npos) return false;

      return std::count(url.begin(), url.end(), opening[bracket]) < std::count(url.begin(), url.end(), p_Ch);
    };

    while ((url.size() > schemeLen) && isTrailing(url.back()))
    {
      url.pop_back();
      Segment& last = segments.back();
      if (--last.m_End == last.m_Start)
      {
        segments.pop_back();
      }
    }

    i = segments.back().m_Line;
    pos = segments.back().m_End;
    if (url.size() <= schemeLen) continue;

    const int id = p_Links.size();
    p_Links.push_back(Link{ url, segments.front().m_Line });
    for (const auto& segment : segments)
    {
      p_Spans[segment.m_Line].push_back(Span{ (int)segment.m_Start, (int)(segment.m_End - segment.m_Start),
                                              p_Attrs, id });
    }
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
                               bool p_IsTable, const Refs& p_Refs, std::vector<Link>* p_Links)
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
  std::vector<bool> literal(len, false); // code spans and autolinks, without emphasis
  std::vector<int> link(len, -1);

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
        literal[k] = true;
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

  // links to reference definitions [text][n] (not [n] alone, often literal), and autolinks <url>
  if (p_Links != nullptr)
  {
    static const size_t maxLinkLen = 2000;
    auto addLink = [&](const std::wstring& p_Url, size_t p_LinkBegin, size_t p_LinkEnd)
    {
      const int id = p_Links->size();
      p_Links->push_back(Link{ p_Url, 0 });
      for (size_t k = p_LinkBegin; k < p_LinkEnd; ++k)
      {
        link[k] = id;
        attrs[k] = CombineAttrs(attrs[k], p_Attrs.m_Link);
      }
    };

    for (size_t pos = 0; pos < len; ++pos)
    {
      if (literal[pos]) continue;

      const size_t maxClose = std::min(len, pos + maxLinkLen);
      size_t end = 0;
      if (text[pos] == L'[')
      {
        // closing bracket, before any nested opening bracket
        size_t close = pos + 1;
        while ((close < maxClose) && (text[close] != L']') && (text[close] != L'['))
        {
          ++close;
        }

        if ((close >= maxClose) || (text[close] != L']') || (close == (pos + 1))) continue;

        const int textNum = ParseRefNum(text, close + 1, end);
        if ((textNum < 0) || !p_Refs.count(textNum) || !IsWebUrl(p_Refs.at(textNum))) continue;

        addLink(p_Refs.at(textNum), pos + 1, close);
        hidden[pos] = true;
        for (size_t k = close; k < end; ++k)
        {
          hidden[k] = true;
        }

        pos = end - 1;
      }
      else if (text[pos] == L'<')
      {
        // url without spaces, newlines from wrapping long url
        std::wstring url;
        size_t close = pos + 1;
        while ((close < maxClose) && (text[close] != L'>') && (text[close] != L'<') &&
               ((text[close] == L'\n') || !iswspace(text[close])))
        {
          if (text[close] != L'\n') url += text[close];

          ++close;
        }

        if ((close >= maxClose) || (text[close] != L'>') || url.empty()) continue;

        const bool isWeb = IsWebUrl(url);
        const bool isMail = !isWeb && (url.find(L'@') != std::wstring::npos) && (url.find(L':') == std::wstring::npos);
        if (!isWeb && !isMail) continue;

        addLink(isMail ? (L"mailto:" + url) : url, pos + 1, close);
        hidden[pos] = true;
        hidden[close] = true;
        for (size_t k = pos + 1; k < close; ++k)
        {
          literal[k] = true;
        }

        pos = close;
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
      if ((text[k] != p_Mark) || literal[k] || hidden[k]) return false;
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
      if ((attrs[pos] == A_NORMAL) && (link[pos] < 0)) continue;

      if (!spans.empty() && (spans.back().m_Attrs == attrs[pos]) && (spans.back().m_Link == link[pos]) &&
          ((spans.back().m_Start + spans.back().m_Len) == col))
      {
        ++spans.back().m_Len;
      }
      else
      {
        spans.push_back(Span{ col, 1, attrs[pos], link[pos] });
      }
    }

    ++pos; // skip newline
    p_Lines[i] = line;
  }
}

// reference definitions [n]: url in last block, removed with blank line before it, and long
// urls wrapped onto next lines, returns begin of lines to remove
size_t TextStyle::ParseRefs(const std::vector<std::wstring>& p_Lines, size_t p_Begin, size_t& p_End,
                            Refs& p_Refs)
{
  size_t end = p_Lines.size();
  while ((end > p_Begin) && p_Lines.at(end - 1).empty())
  {
    --end;
  }

  size_t begin = end;
  while ((begin > p_Begin) && !p_Lines.at(begin - 1).empty())
  {
    --begin;
  }

  Refs refs;
  int num = -1;
  for (size_t i = begin; i < end; ++i)
  {
    const std::wstring& line = p_Lines.at(i);
    size_t pos = 0;
    const int lineNum = ParseRefNum(line, 0, pos);
    const bool isDef = (lineNum >= 0) && (pos < line.size()) && (line[pos] == L':') &&
      (((pos + 1) == line.size()) || (line[pos + 1] == L' '));
    const std::wstring url = isDef ? line.substr(std::min(pos + 2, line.size())) : line;
    if ((!isDef && (num < 0)) || (url.find(L' ') != std::wstring::npos)) return p_Lines.size();

    if (isDef)
    {
      num = lineNum;
    }

    refs[num] += url;
  }

  if (refs.empty()) return p_Lines.size();

  p_Refs = refs;
  p_End = end;
  return ((begin > p_Begin) && p_Lines.at(begin - 1).empty()) ? (begin - 1) : begin;
}

// reference number [n] at p_Pos, or -1
int TextStyle::ParseRefNum(const std::wstring& p_Text, size_t p_Pos, size_t& p_End)
{
  if ((p_Pos >= p_Text.size()) || (p_Text[p_Pos] != L'[')) return -1;

  static const size_t maxDigits = 6;
  int num = 0;
  size_t pos = p_Pos + 1;
  while ((pos < p_Text.size()) && iswdigit(p_Text[pos]) && ((pos - p_Pos) <= maxDigits))
  {
    num = (num * 10) + (p_Text[pos] - L'0');
    ++pos;
  }

  if ((pos == (p_Pos + 1)) || (pos >= p_Text.size()) || (p_Text[pos] != L']')) return -1;

  p_End = pos + 1;
  return num;
}

// http(s) or mailto url, as other schemes may open local files or apps
bool TextStyle::IsWebUrl(const std::wstring& p_Url)
{
  return (p_Url.rfind(L"http://", 0) == 0) || (p_Url.rfind(L"https://", 0) == 0) ||
         (p_Url.rfind(L"mailto:", 0) == 0);
}

// length of url scheme at p_Pos, not preceded by letter or digit, or 0
size_t TextStyle::ParseUrlScheme(const std::wstring& p_Line, size_t p_Pos)
{
  static const std::vector<std::wstring> schemes = { L"http://", L"https://", L"mailto:" };

  if ((p_Pos > 0) && iswalnum(p_Line[p_Pos - 1])) return 0;

  for (const auto& scheme : schemes)
  {
    if ((p_Line.size() - p_Pos) < scheme.size()) continue;

    bool match = true;
    for (size_t k = 0; match && (k < scheme.size()); ++k)
    {
      match = (towlower(p_Line[p_Pos + k]) == scheme[k]);
    }

    if (match) return scheme.size();
  }

  return 0;
}

// printable ascii, except delimiters commonly enclosing urls
bool TextStyle::IsUrlChar(wchar_t p_Ch)
{
  return (p_Ch > 0x20) && (p_Ch < 0x7f) && (p_Ch != L'<') && (p_Ch != L'>') && (p_Ch != L'"') &&
         (p_Ch != L'`');
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
