// lineedit.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class LineEdit
{
public:
  static bool Read(const std::string& p_Prompt, std::string& p_Value, bool p_ReplaceOnType = false);

private:
  static bool ReadPlain(const std::string& p_Prompt, std::string& p_Value);
  static bool ReadEdit(const std::string& p_Prompt, std::string& p_Value, bool p_ReplaceOnType);
  static int ReadByte(int p_TimeoutMs = -1);
  static void Redraw(const std::string& p_Prompt, const std::wstring& p_Text, size_t p_Cursor);
};
