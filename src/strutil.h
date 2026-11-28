// strutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

class StrUtil
{
public:
  static void ReplaceString(std::string& p_Str, const std::string& p_Search,
                            const std::string& p_Replace);
  static size_t ReplaceStringCount(std::string& p_Str, const std::string& p_Search,
                                   const std::string& p_Replace);
  static bool ReplaceStringFirst(std::string& p_Str, const std::string& p_Search,
                                 const std::string& p_Replace);
  static std::string ToString(const std::wstring& p_WStr);
  static std::wstring ToWString(const std::string& p_Str);
  static std::string TrimPadString(const std::string& p_Str, int p_Len);
  static size_t GetClusterEnd(const std::wstring& p_Str, size_t p_Pos);
  static std::wstring TrimWString(const std::wstring& p_Str, int p_Len, int* p_Width = nullptr);
  static std::wstring TrimPadWString(const std::wstring& p_Str, int p_Len);
  static std::wstring TrimEllipsisWString(const std::wstring& p_Str, int p_Len);
  static std::string RemoveInvisibleChars(const std::string& p_Str);
  static std::wstring RemoveInvisibleChars(const std::wstring& p_WStr);
  static int WStringWidth(const std::wstring& p_WStr);

  template<typename T>
  static inline T Bound(const T& p_Min, const T& p_Val, const T& p_Max)
  {
    return std::max(p_Min, std::min(p_Val, p_Max));
  }

  static std::string ToFold(const std::string& p_Str);
  static std::wstring ToFold(const std::wstring& p_WStr, std::vector<size_t>* p_Positions = nullptr);
  static std::string ToLower(const std::string& p_Str);
  static std::wstring ToLower(const std::wstring& p_WStr);
  static std::vector<std::string> Split(const std::string& p_Str, char p_Sep = ',');
  static std::vector<std::string> SplitQuoted(const std::string& p_Str, bool p_Unquote);
  static std::string Trim(const std::string& p_Str);
  static std::vector<std::string> Trim(const std::vector<std::string>& p_Strs);
  static std::wstring Join(const std::vector<std::wstring>& p_Lines);
  static std::string Join(const std::vector<std::string>& p_Lines,
                          const std::string& p_Delim = "\n");
  static std::string ToHexString(int p_Val);
  static std::string StrFromOct(const std::string& p_Str);
  static bool IsInteger(const std::string& p_Str);
  static long ToInteger(const std::string& p_Str);
  static void StripCR(std::wstring& p_Str);
  static std::string Strip(const std::string& p_Str, const char p_Char);
  static std::string TrimLeft(const std::string& p_Str, const std::string& p_Trim);
  static std::string TrimRight(const std::string& p_Str, const std::string& p_Trim);
  static void BitInvertString(std::string& p_String);
  static std::string ZeroPad(uint32_t p_Num, int32_t p_Len);
  static std::string ToHex(const std::string& p_String);
  static std::string FromHex(const std::string& p_String);
  static void RemoveChar(std::string& p_Str, char p_Char);
  static void RemoveNonAlphaNumSpace(std::string& p_Str);
  static std::string ExtractString(const std::string& p_Str, const std::string& p_Prefix, const std::string& p_Suffix);
  static bool StartsWith(const std::string& p_String, const std::string& p_Prefix);
};
