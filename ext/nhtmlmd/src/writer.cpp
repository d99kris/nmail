// writer.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#include "writer.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "util.h"

namespace Nhtmlmd
{
  static Writer::Op MakeOp(Writer::Op::Type p_Type, const std::string& p_Str, const std::string& p_Rest,
                           int p_Blank)
  {
    Writer::Op op;
    op.m_Type = p_Type;
    op.m_Str = p_Str;
    op.m_Rest = p_Rest;
    op.m_Blank = p_Blank;
    return op;
  }

  void Writer::SetRecord(bool p_Record)
  {
    m_Record = p_Record;
  }

  std::vector<Writer::Op> Writer::TakeOps()
  {
    return std::move(m_Ops);
  }

  void Writer::Replay(const std::vector<Op>& p_Ops)
  {
    for (const auto& op : p_Ops)
    {
      switch (op.m_Type)
      {
        case Op::Text:
          Text(op.m_Str);
          break;

        case Op::Pre:
          Pre(op.m_Str);
          break;

        case Op::Literal:
          Literal(op.m_Str);
          break;

        case Op::LineBreak:
          LineBreak();
          break;

        case Op::Space:
          Space();
          break;

        case Op::Block:
          Block(op.m_Blank);
          break;

        case Op::PushPrefix:
          PushPrefix(op.m_Str, op.m_Rest);
          break;

        case Op::PopPrefix:
          PopPrefix();
          break;

        case Op::Superscript:
          Superscript(op.m_Str, op.m_Short, op.m_Ops);
          break;

        default:
          break;
      }
    }
  }

  // collapses whitespace, drops invisible padding
  void Writer::Text(const std::string& p_Str)
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::Text, p_Str, "", 0));

    for (size_t i = 0; i < p_Str.size(); )
    {
      uint32_t cp = 0;
      const size_t len = Util::DecodeUtf8(p_Str, i, cp);
      if (Util::IsJunk(cp))
      {
        // skip
      }
      else if (Util::IsSpace(cp))
      {
        m_PendingSpace = true;
        // block with only nbsp is an intentional empty line
        if ((cp == 0x00A0) && !m_HasContent) m_NbspOnly = true;
      }
      else
      {
        if (m_PendingSpace && m_HasContent) m_Line += ' ';
        m_PendingSpace = false;
        m_Line.append(p_Str, i, len);
        m_HasContent = true;
      }

      i += len;
    }
  }

  // keeps whitespace and newlines
  void Writer::Pre(const std::string& p_Str)
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::Pre, p_Str, "", 0));

    for (char ch : p_Str)
    {
      if (ch == '\r') continue;

      if (ch == '\n')
      {
        EmitLine(true);
      }
      else
      {
        m_Line += ch;
        m_HasContent = true;
      }
    }
  }

  // complete line of formatted text
  void Writer::Literal(const std::string& p_Str)
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::Literal, p_Str, "", 0));

    if (m_HasContent) EmitLine(false);

    if (p_Str.empty())
    {
      m_PendingBlank = std::max(m_PendingBlank, 1);
      return;
    }

    m_Line = p_Str;
    m_HasContent = true;
    EmitLine(false);
  }

  void Writer::LineBreak()
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::LineBreak, "", "", 0));

    if (m_HasContent)
    {
      EmitLine(false);
    }
    else
    {
      m_PendingBlank = std::max(m_PendingBlank, 1);
    }

    m_PendingSpace = false;
  }

  void Writer::Space()
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::Space, "", "", 0));

    m_PendingSpace = true;
  }

  void Writer::Block(int p_Blank)
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::Block, "", "", p_Blank));

    if (!m_HasContent && m_NbspOnly) p_Blank = std::max(p_Blank, 1);

    m_NbspOnly = false;
    if (m_HasContent) EmitLine(false);

    if (m_Lines.empty() && (p_Blank > 0)) m_LeadingBlank = true;

    m_PendingBlank = std::max(m_PendingBlank, p_Blank);
    m_PendingSpace = false;
  }

  void Writer::PushPrefix(const std::string& p_First, const std::string& p_Rest)
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::PushPrefix, p_First, p_Rest, 0));

    m_Prefixes.push_back(Prefix{ p_First, p_Rest, false });
  }

  void Writer::PopPrefix()
  {
    if (m_Record) m_Ops.push_back(MakeOp(Op::PopPrefix, "", "", 0));

    m_Prefixes.pop_back();
  }

  // short superscript as ^x, except at line start or after digit, decided by writer content
  // also on replay, as when walked
  void Writer::Superscript(const std::string& p_Text, bool p_Short, std::vector<Op> p_Ops)
  {
    const char last = GetLastChar();
    const bool caret = p_Short && (last != '\0') && !isdigit((unsigned char)last);
    const bool record = m_Record;
    m_Record = false;
    if (caret)
    {
      Text("^" + p_Text);
    }
    else
    {
      Replay(p_Ops);
    }

    m_Record = record;
    if (m_Record)
    {
      Op op = MakeOp(Op::Superscript, p_Text, "", 0);
      op.m_Short = p_Short;
      op.m_Ops = std::move(p_Ops);
      m_Ops.push_back(std::move(op));
    }
  }

  char Writer::GetLastChar() const
  {
    if (!m_HasContent) return '\0';

    if (m_PendingSpace) return ' ';

    return m_Line.back();
  }

  bool Writer::GetLeadingBlank() const
  {
    return m_LeadingBlank;
  }

  bool Writer::GetTrailingBlank() const
  {
    return m_TrailingBlank;
  }

  std::string Writer::Finish()
  {
    if (m_HasContent) EmitLine(false);

    m_TrailingBlank = (m_PendingBlank > 0);

    std::string str;
    bool prevBlank = true;
    for (auto& line : m_Lines)
    {
      while (!line.empty() && (line.back() == ' '))
      {
        line.pop_back();
      }

      const bool blank = (line.find_first_not_of("> ") == std::string::npos);
      if (blank && prevBlank) continue;

      str += line + "\n";
      prevBlank = blank;
    }

    while ((str.size() >= 2) && (str[str.size() - 1] == '\n') && (str[str.size() - 2] == '\n'))
    {
      str.pop_back();
    }

    return str;
  }

  void Writer::EmitLine(bool p_Force)
  {
    if (!m_HasContent && !p_Force) return;

    if ((m_PendingBlank > 0) && !m_Lines.empty())
    {
      // blank lines keep started quote prefixes
      std::string blank;
      for (const auto& prefix : m_Prefixes)
      {
        if (!prefix.m_Used) break;

        blank += prefix.m_Rest;
      }

      while (!blank.empty() && (blank.back() == ' '))
      {
        blank.pop_back();
      }

      for (int i = 0; i < m_PendingBlank; ++i)
      {
        m_Lines.push_back(blank);
      }
    }

    m_PendingBlank = 0;

    std::string prefixStr;
    for (auto& prefix : m_Prefixes)
    {
      prefixStr += prefix.m_Used ? prefix.m_Rest : prefix.m_First;
      prefix.m_Used = true;
    }

    m_Lines.push_back(prefixStr + m_Line);
    m_Line.clear();
    m_HasContent = false;
    m_NbspOnly = false;
    m_PendingSpace = false;
  }
}
