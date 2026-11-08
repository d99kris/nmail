// node.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Nhtmlmd
{
  struct Node
  {
    enum Type { Element, Text };

    Type m_Type = Element;
    std::string m_Tag;
    std::string m_Text;
    std::vector<std::pair<std::string, std::string>> m_Attrs;
    std::vector<std::unique_ptr<Node>> m_Children;

    const std::string* Attr(const char* p_Name) const
    {
      for (const auto& attr : m_Attrs)
      {
        if (attr.first == p_Name) return &attr.second;
      }

      return nullptr;
    }

    Node* AddChild(Type p_Type)
    {
      m_Children.emplace_back(new Node());
      m_Children.back()->m_Type = p_Type;
      return m_Children.back().get();
    }
  };
}
