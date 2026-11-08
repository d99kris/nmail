// writer.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

namespace Nhtmlmd
{
  class Writer
  {
  public:
    void Text(const std::string& p_Str);
    void Pre(const std::string& p_Str);
    void Literal(const std::string& p_Str);
    void LineBreak();
    void Space();
    void Block(int p_Blank);
    void PushPrefix(const std::string& p_First, const std::string& p_Rest);
    void PopPrefix();
    char GetLastChar() const;
    bool GetLeadingBlank() const;
    bool GetTrailingBlank() const;
    std::string Finish();

  private:
    void EmitLine(bool p_Force);

  private:
    struct Prefix
    {
      std::string m_First;
      std::string m_Rest;
      bool m_Used = false;
    };

    std::vector<Prefix> m_Prefixes;
    std::vector<std::string> m_Lines;
    std::string m_Line;
    bool m_HasContent = false;
    bool m_PendingSpace = false;
    int m_PendingBlank = 0;
    bool m_NbspOnly = false;
    bool m_LeadingBlank = false;
    bool m_TrailingBlank = false;
  };
}
