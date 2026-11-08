// walker.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "nhtmlmd.h"
#include "node.h"
#include "writer.h"

namespace Nhtmlmd
{
  // tags skipped with their content
  bool IsSkipTag(const std::string& p_Tag);

  class Walker
  {
  public:
    Walker(const HtmlOptions& p_Options, bool p_Markdown);
    std::string Run(const Node& p_Root);

  private:
    struct Cell
    {
      std::string m_Text;
      bool m_BlankBefore = false;
      bool m_BlankAfter = false;
      bool m_Header = false;
      bool m_SingleLine = true;
      const Node* m_Node = nullptr;
    };

    struct List
    {
      bool m_Ordered = false;
      int m_Next = 1;
    };

    void Walk(const Node& p_Node);
    void WalkChildren(const Node& p_Node);
    void Heading(const Node& p_Node);
    void Pre(const Node& p_Node);
    void Emphasis(const Node& p_Node, const std::string& p_Mark);
    void Superscript(const Node& p_Node);
    void Image(const Node& p_Node);
    void Link(const Node& p_Node);
    bool MarkdownLink(const Node& p_Node, std::vector<Writer::Op>& p_Ops, bool& p_Rendered,
                      bool& p_Empty);
    bool IsRenderDeep() const;
    size_t LinkIndex(const std::string& p_Href);
    std::string RenderSub(const Node& p_Node, bool* p_BlankBefore = nullptr, bool* p_BlankAfter = nullptr,
                          std::vector<Writer::Op>* p_Ops = nullptr);
    void Table(const Node& p_Node);
    void LayoutTable(const std::vector<std::vector<Cell>>& p_Cells);
    bool GridTable(const std::vector<std::vector<Cell>>& p_Cells);
    void CellLines(const Cell& p_Cell, int p_Sep);

  private:
    const HtmlOptions& m_Options;
    const bool m_Markdown;
    Writer* m_Writer = nullptr;
    int m_InPre = 0;
    int m_RenderDepth = 0;
    std::vector<List> m_Lists;
    std::vector<bool> m_LinkHasText;
    std::vector<std::string> m_Links;
    std::map<std::string, size_t> m_LinkIndex;
  };
}
