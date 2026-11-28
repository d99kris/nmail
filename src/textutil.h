// textutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

class TextUtil
{
public:
  static std::string ReduceIndent(const std::string& p_Str, int p_Cnt);
  static std::string AddIndent(const std::string& p_Str, const std::string& p_Indent);
  static std::vector<std::wstring> WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                            bool p_ProcessFormatFlowed, bool p_OutputFormatFlowed,
                                            bool p_QuoteWrap, int p_ExpandTabSize);
  static std::vector<std::wstring> WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                            bool p_ProcessFormatFlowed, bool p_OutputFormatFlowed,
                                            bool p_QuoteWrap, int p_ExpandTabSize,
                                            int p_Pos, int& p_WrapLine, int& p_WrapPos,
                                            std::vector<int>* p_LineOffsets = nullptr);
  static bool GetQuotePrefix(const std::string& p_String, std::string& p_Prefix, std::string& p_Line);
  static bool GetQuotePrefix(const std::wstring& p_String, std::wstring& p_Prefix, std::wstring& p_Line);
  static void DeleteToNextMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars);
  static void DeleteToPrevMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars);
  static void JumpToNextMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars);
  static void JumpToPrevMatch(std::wstring& p_Str, int& p_Pos, int p_Offs, std::wstring p_Chars);
  static std::string UnwrapQuotedLines(const std::string& p_Msg, int p_LineLen);
};
