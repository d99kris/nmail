// strutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "strutil.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <sstream>

#include <utf8.h>

#include "loghelp.h"
#include "textwrap.h"

void StrUtil::ReplaceString(std::string& p_Str, const std::string& p_Search,
                            const std::string& p_Replace)
{
  (void)ReplaceStringCount(p_Str, p_Search, p_Replace);
}

size_t StrUtil::ReplaceStringCount(std::string& p_Str, const std::string& p_Search,
                                   const std::string& p_Replace)
{
  size_t cnt = 0;
  size_t pos = 0;
  while ((pos = p_Str.find(p_Search, pos)) != std::string::npos)
  {
    p_Str.replace(pos, p_Search.length(), p_Replace);
    pos += p_Replace.length();
    ++cnt;
  }

  return cnt;
}

bool StrUtil::ReplaceStringFirst(std::string& p_Str, const std::string& p_Search,
                                 const std::string& p_Replace)
{
  size_t pos = 0;
  if ((pos = p_Str.find(p_Search, pos)) != std::string::npos)
  {
    p_Str.replace(pos, p_Search.length(), p_Replace);
    return true;
  }

  return false;
}

std::string StrUtil::ToString(const std::wstring& p_WStr)
{
  try
  {
    std::string result;
    std::u32string u32str(p_WStr.begin(), p_WStr.end());
    utf8::utf32to8(u32str.begin(), u32str.end(), std::back_inserter(result));
    return result;
  }
  catch (...)
  {
    LOG_WARNING("error converting to utf-8");
    std::wstring wstr = p_WStr;
    wstr.erase(std::remove_if(wstr.begin(), wstr.end(), [](wchar_t wch) { return !isascii(wch); }), wstr.end());
    return std::string(wstr.begin(), wstr.end());
  }
}

std::wstring StrUtil::ToWString(const std::string& p_Str)
{
  try
  {
    std::string valid = utf8::replace_invalid(p_Str);
    if (valid != p_Str)
    {
      LOG_WARNING("error converting from utf-8");
    }

    std::u32string u32str;
    utf8::utf8to32(valid.begin(), valid.end(), std::back_inserter(u32str));
    return std::wstring(u32str.begin(), u32str.end());
  }
  catch (...)
  {
    LOG_WARNING("error converting from utf-8");
    std::string str = p_Str;
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char ch) { return !isascii(ch); }), str.end());
    return std::wstring(str.begin(), str.end());
  }
}

std::string StrUtil::TrimPadString(const std::string& p_Str, int p_Len)
{
  p_Len = std::max(p_Len, 0);
  std::string str = p_Str;
  if ((int)str.size() > p_Len)
  {
    str = str.substr(0, p_Len);
  }
  else if ((int)str.size() < p_Len)
  {
    str = str + std::string(p_Len - str.size(), ' ');
  }
  return str;
}

size_t StrUtil::GetClusterEnd(const std::wstring& p_Str, size_t p_Pos)
{
  return TextWrap::GetClusterEnd(p_Str, p_Pos);
}

// Returns the longest prefix of p_Str with a display width of at most p_Len, never
// splitting a character cluster. Display width may differ from character count for
// strings containing zero-width or double-width characters.
std::wstring StrUtil::TrimWString(const std::wstring& p_Str, int p_Len, int* p_Width /*= nullptr*/)
{
  p_Len = std::max(p_Len, 0);
  int width = 0;
  size_t pos = 0;
  while (pos < p_Str.size())
  {
    const size_t end = GetClusterEnd(p_Str, pos);
    int clusterWidth = 0;
    for (size_t i = pos; i < end; ++i)
    {
      clusterWidth += std::max(wcwidth(p_Str[i]), 0);
    }

    if ((width + clusterWidth) > p_Len) break;

    width += clusterWidth;
    pos = end;
  }

  if (p_Width != nullptr)
  {
    *p_Width = width;
  }

  return p_Str.substr(0, pos);
}

std::wstring StrUtil::TrimPadWString(const std::wstring& p_Str, int p_Len)
{
  p_Len = std::max(p_Len, 0);

  int width = 0;
  std::wstring str = TrimWString(p_Str, p_Len, &width);
  if (width < p_Len)
  {
    str = str + std::wstring(p_Len - width, ' ');
  }

  return str;
}

// trim to display width, with trailing ellipsis if trimmed
std::wstring StrUtil::TrimEllipsisWString(const std::wstring& p_Str, int p_Len)
{
  p_Len = std::max(p_Len, 0);
  if (WStringWidth(p_Str) <= p_Len) return p_Str;

  static const std::wstring ellipsis = L"...";
  const int maxWidth = std::max(p_Len - (int)ellipsis.size(), 0);
  int width = 0;
  std::wstring str = TrimWString(p_Str, maxWidth, &width);

  return str + ellipsis.substr(0, p_Len - width);
}

std::string StrUtil::RemoveInvisibleChars(const std::string& p_Str)
{
  return ToString(RemoveInvisibleChars(ToWString(p_Str)));
}

std::wstring StrUtil::RemoveInvisibleChars(const std::wstring& p_WStr)
{
  // remove format characters which have no visible representation. terminals disagree
  // whether to allocate a cell for them (apple terminal does, while wcwidth reports
  // zero width), which misaligns any text following them on the same row.
  static const auto isInvisible = [](wchar_t p_WCh)
  {
    return ((p_WCh == 0x00ad) || // soft hyphen
            ((p_WCh >= 0x200b) && (p_WCh <= 0x200f)) || // zero-width space .. right-to-left mark
            ((p_WCh >= 0x202a) && (p_WCh <= 0x202e)) || // bidi embeddings and overrides
            ((p_WCh >= 0x2060) && (p_WCh <= 0x2064)) || // word joiner and invisible operators
            ((p_WCh >= 0x2066) && (p_WCh <= 0x2069)) || // bidi isolates
            (p_WCh == 0xfeff)); // zero-width no-break space
  };

  std::wstring wstr = p_WStr;
  wstr.erase(std::remove_if(wstr.begin(), wstr.end(), isInvisible), wstr.end());
  return wstr;
}

int StrUtil::WStringWidth(const std::wstring& p_WStr)
{
  int width = wcswidth(p_WStr.c_str(), p_WStr.size());
  return (width != -1) ? width : p_WStr.size();
}

// Base letter folding table for Latin-1 Supplement (U+00C0 - U+00FF) and Latin Extended-A
// (U+0100 - U+017F), mapping each code point to its lowercase ascii base letter(s). Empty
// string means the code point is left as-is (the two math symbols in the range). Limited to
// these two blocks by design, leaving cjk, greek, cyrillic, etc untouched by base folding.
static const char* const s_LatinFold[] =
{
  // U+00C0 - U+00DF
  "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
  "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u", "u", "y", "th", "ss",
  // U+00E0 - U+00FF
  "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
  "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u", "u", "y", "th", "y",
  // U+0100 - U+011F
  "a", "a", "a", "a", "a", "a", "c", "c", "c", "c", "c", "c", "c", "c", "d", "d",
  "d", "d", "e", "e", "e", "e", "e", "e", "e", "e", "e", "e", "g", "g", "g", "g",
  // U+0120 - U+013F
  "g", "g", "g", "g", "h", "h", "h", "h", "i", "i", "i", "i", "i", "i", "i", "i",
  "i", "i", "ij", "ij", "j", "j", "k", "k", "k", "l", "l", "l", "l", "l", "l", "l",
  // U+0140 - U+015F
  "l", "l", "l", "n", "n", "n", "n", "n", "n", "n", "n", "n", "o", "o", "o", "o",
  "o", "o", "oe", "oe", "r", "r", "r", "r", "r", "r", "s", "s", "s", "s", "s", "s",
  // U+0160 - U+017F
  "s", "s", "t", "t", "t", "t", "t", "t", "u", "u", "u", "u", "u", "u", "u", "u",
  "u", "u", "u", "u", "w", "w", "y", "y", "y", "z", "z", "z", "z", "z", "z", "s",
};

std::string StrUtil::ToFold(const std::string& p_Str)
{
  // Fast path for ascii-only strings, which need neither decoding nor base letter folding
  if (std::none_of(p_Str.begin(), p_Str.end(),
                   [](char ch) { return static_cast<unsigned char>(ch) >= 0x80; }))
  {
    return ToLower(p_Str);
  }

  return ToString(ToFold(ToWString(p_Str)));
}

std::wstring StrUtil::ToFold(const std::wstring& p_WStr, std::vector<size_t>* p_Positions /*= nullptr*/)
{
  static const wchar_t foldFirst = 0x00C0;
  static const wchar_t foldLast = foldFirst + (sizeof(s_LatinFold) / sizeof(*s_LatinFold)) - 1;

  // Optionally outputs the source position of each folded char, as folding may change length
  std::wstring fold;
  fold.reserve(p_WStr.size());
  for (size_t i = 0; i < p_WStr.size(); ++i)
  {
    const wchar_t wch = p_WStr[i];
    if ((wch >= foldFirst) && (wch <= foldLast) && (s_LatinFold[wch - foldFirst][0] != '\0'))
    {
      for (const char* base = s_LatinFold[wch - foldFirst]; *base != '\0'; ++base)
      {
        fold += static_cast<wchar_t>(*base);
      }
    }
    else
    {
      // Handles case folding for scripts outside the table (greek, cyrillic, etc), based on
      // the locale set up by the ui. Code points without case mapping are left as-is.
      fold += static_cast<wchar_t>(towlower(static_cast<wint_t>(wch)));
    }

    if (p_Positions != nullptr)
    {
      p_Positions->resize(fold.size(), i);
    }
  }

  return fold;
}

// Ascii-only, as it operates on single bytes / code points. Use ToFold() for search matching.
std::string StrUtil::ToLower(const std::string& p_Str)
{
  std::string lower = p_Str;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](char ch) { return static_cast<char>(::tolower(static_cast<unsigned char>(ch))); });
  return lower;
}

std::wstring StrUtil::ToLower(const std::wstring& p_WStr)
{
  std::wstring lower = p_WStr;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](wchar_t wch) { return static_cast<wchar_t>(towlower(static_cast<wint_t>(wch))); });
  return lower;
}

std::vector<std::string> StrUtil::Split(const std::string& p_Str, char p_Sep)
{
  std::vector<std::string> vec;
  if (!p_Str.empty())
  {
    std::stringstream ss(p_Str);
    while (ss.good())
    {
      std::string str;
      getline(ss, str, p_Sep);
      vec.push_back(str);
    }
  }
  return vec;
}

std::vector<std::string> StrUtil::SplitQuoted(const std::string& p_Str, bool p_Unquote)
{
  std::vector<std::string> vec;
  if (!p_Str.empty())
  {
    std::stringstream ss(p_Str);
    while (ss >> std::ws)
    {
      std::string str;
      if (ss.peek() == '"')
      {
        if (p_Unquote)
        {
          ss >> std::quoted(str);
        }
        else
        {
          ss >> str;
        }
        std::string extra;
        std::getline(ss, extra, ',');
        str += extra;
      }
      else
      {
        std::getline(ss, str, ',');
      }

      str = Trim(str);
      if (!str.empty())
      {
        vec.push_back(str);
      }
    }
  }

  return vec;
}

std::string StrUtil::Trim(const std::string& p_Str)
{
  std::string space = " ";
  const auto strBegin = p_Str.find_first_not_of(space);
  if (strBegin == std::string::npos) return "";

  const auto strEnd = p_Str.find_last_not_of(space);
  const auto strRange = strEnd - strBegin + 1;

  return p_Str.substr(strBegin, strRange);
}

std::vector<std::string> StrUtil::Trim(const std::vector<std::string>& p_Strs)
{
  std::vector<std::string> trimStrs;
  for (auto& str : p_Strs)
  {
    trimStrs.push_back(Trim(str));
  }

  return trimStrs;
}

std::wstring StrUtil::Join(const std::vector<std::wstring>& p_Lines)
{
  std::wstring str;
  bool first = true;
  for (auto& line : p_Lines)
  {
    if (!first)
    {
      str += L"\n";
    }
    else
    {
      first = false;
    }

    str += line;
  }
  return str;
}

std::string StrUtil::Join(const std::vector<std::string>& p_Lines, const std::string& p_Delim)
{
  std::string str;
  bool first = true;
  for (auto& line : p_Lines)
  {
    if (!first)
    {
      str += p_Delim;
    }
    else
    {
      first = false;
    }

    str += line;
  }
  return str;
}

std::string StrUtil::ToHexString(int p_Val)
{
  std::stringstream stream;
  stream << std::hex << std::uppercase << p_Val;
  return "0x" + stream.str();
}

std::string StrUtil::StrFromOct(const std::string& p_Str)
{
  std::string rv;
  std::vector<std::string> parts = Split(p_Str, '\\');
  for (auto& part : parts)
  {
    if (part.empty()) continue;

    int val = 0;
    std::istringstream(part) >> std::oct >> val;
    rv += (char)val;
  }

  return rv;
}

bool StrUtil::IsInteger(const std::string& p_Str)
{
  return (p_Str.find_first_not_of("0123456789") == std::string::npos);
}

long StrUtil::ToInteger(const std::string& p_Str)
{
  return strtol(p_Str.c_str(), NULL, 10);
}

void StrUtil::StripCR(std::wstring& p_Str)
{
  p_Str.erase(std::remove(p_Str.begin(), p_Str.end(), L'\r'), p_Str.end());
}

std::string StrUtil::Strip(const std::string& p_Str, const char p_Char)
{
  std::string str = p_Str;
  str.erase(std::remove(str.begin(), str.end(), p_Char), str.end());
  return str;
}

std::string StrUtil::TrimLeft(const std::string& p_Str, const std::string& p_Trim)
{
  std::string str = p_Str;
  str.erase(0, str.find_first_not_of(p_Trim));
  return str;
}

std::string StrUtil::TrimRight(const std::string& p_Str, const std::string& p_Trim)
{
  const auto strEnd = p_Str.find_last_not_of(p_Trim);

  return p_Str.substr(0, strEnd + 1);
}

void StrUtil::BitInvertString(std::string& p_String)
{
  for (auto& ch : p_String)
  {
    ch = ~ch;
  }
}

std::string StrUtil::ZeroPad(uint32_t p_Num, int32_t p_Len)
{
  std::string str = std::to_string(p_Num);
  int32_t zeroCount = std::max(0, p_Len - (int)str.length());
  str = std::string(zeroCount, '0') + str;
  return str;
}

std::string StrUtil::ToHex(const std::string& p_String)
{
  std::ostringstream oss;
  for (const char& ch : p_String)
  {
    char buf[3] = { 0 };
    snprintf(buf, sizeof(buf), "%02X", (unsigned char)ch);
    oss << buf;
  }

  return oss.str();
}

std::string StrUtil::FromHex(const std::string& p_String)
{
  std::string result;
  std::istringstream iss(p_String);
  char buf[3] = { 0 };
  while (iss.read(buf, 2))
  {
    result += static_cast<char>(strtol(buf, NULL, 16) & 0xff);
  }

  return result;
}

void StrUtil::RemoveChar(std::string& p_Str, char p_Char)
{
  p_Str.erase(std::remove(p_Str.begin(), p_Str.end(), p_Char), p_Str.end());
}

void StrUtil::RemoveNonAlphaNumSpace(std::string& p_Str)
{
  auto it = std::remove_if(p_Str.begin(), p_Str.end(), [](char const& c) {
    return !std::isalnum(c) && (c != ' ');
  });

  p_Str.erase(it, p_Str.end());
}

std::string StrUtil::ExtractString(const std::string& p_Str, const std::string& p_Prefix, const std::string& p_Suffix)
{
  std::size_t prefixPos = p_Str.find(p_Prefix);
  if (prefixPos != std::string::npos)
  {
    std::size_t suffixPos = p_Str.find(p_Suffix, prefixPos + p_Prefix.size());
    std::size_t len = (suffixPos != std::string::npos) ? (suffixPos - prefixPos - p_Prefix.size()) : std::string::npos;
    return p_Str.substr(prefixPos + p_Prefix.size(), len);
  }

  return "";
}

bool StrUtil::StartsWith(const std::string& p_String, const std::string& p_Prefix)
{
  return (p_String.rfind(p_Prefix, 0) == 0);
}
