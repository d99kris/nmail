// nhtmlmd.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#include "nhtmlmd.h"

#include <map>

#include <lexbor/dom/interfaces/character_data.h>
#include <lexbor/dom/interfaces/text.h>
#include <lexbor/html/html.h>
#include <md4c-html.h>

#include "node.h"
#include "walker.h"

namespace Nhtmlmd
{
  static void BuildNode(lxb_dom_node_t* p_Src, Node* p_Dst)
  {
    for (lxb_dom_node_t* child = p_Src->first_child; child != nullptr; child = child->next)
    {
      if (child->type == LXB_DOM_NODE_TYPE_ELEMENT)
      {
        lxb_dom_element_t* element = lxb_dom_interface_element(child);
        Node* node = p_Dst->AddChild(Node::Element);
        size_t len = 0;
        const lxb_char_t* name = lxb_dom_element_local_name(element, &len);
        node->m_Tag.assign((const char*)name, len);
        for (lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(element); attr != nullptr;
             attr = lxb_dom_element_next_attribute(attr))
        {
          size_t nameLen = 0;
          size_t valueLen = 0;
          const lxb_char_t* attrName = lxb_dom_attr_local_name(attr, &nameLen);
          const lxb_char_t* attrValue = lxb_dom_attr_value(attr, &valueLen);
          node->m_Attrs.emplace_back(std::string((const char*)attrName, nameLen),
                                     (attrValue != nullptr) ? std::string((const char*)attrValue, valueLen)
                                                            : std::string());
        }

        BuildNode(child, node);
      }
      else if (child->type == LXB_DOM_NODE_TYPE_TEXT)
      {
        lxb_dom_text_t* text = lxb_dom_interface_text(child);
        Node* node = p_Dst->AddChild(Node::Text);
        node->m_Text.assign((const char*)text->char_data.data.data, text->char_data.data.length);
      }
    }
  }

  static bool ParseHtml(const std::string& p_Html, Node& p_Root)
  {
    lxb_html_document_t* document = lxb_html_document_create();
    if (document == nullptr) return false;

    const lxb_status_t status =
      lxb_html_document_parse(document, (const lxb_char_t*)p_Html.data(), p_Html.size());
    if (status == LXB_STATUS_OK)
    {
      BuildNode(lxb_dom_interface_node(document), &p_Root);
    }

    lxb_html_document_destroy(document);
    return (status == LXB_STATUS_OK);
  }

  static void OnHtmlOutput(const MD_CHAR* p_Data, MD_SIZE p_Size, void* p_UserData)
  {
    static_cast<std::string*>(p_UserData)->append(p_Data, p_Size);
  }

  // inline styles, as head css is often dropped by clients and in quotes
  static std::string AddStyles(const std::string& p_Html)
  {
    static const std::map<std::string, std::string> styles =
    {
      { "blockquote", "margin:0 0 0 0.8ex;padding-left:1ex;border-left:1px solid #cccccc;color:#555555" },
      { "code", "font-family:Menlo,Consolas,monospace;font-size:90%" },
      { "pre", "font-family:Menlo,Consolas,monospace;font-size:90%;background:#f6f8fa;padding:8px;"
        "overflow-x:auto" },
      { "table", "border-collapse:collapse" },
      { "td", "border:1px solid #cccccc;padding:3px 8px" },
      { "th", "border:1px solid #cccccc;padding:3px 8px" },
    };

    std::string html;
    size_t pos = 0;
    while (pos < p_Html.size())
    {
      const size_t lt = p_Html.find('<', pos);
      if (lt == std::string::npos)
      {
        html += p_Html.substr(pos);
        break;
      }

      size_t end = lt + 1;
      while ((end < p_Html.size()) && isalpha((unsigned char)p_Html[end]))
      {
        ++end;
      }

      html += p_Html.substr(pos, end - pos);
      const std::string tag = p_Html.substr(lt + 1, end - lt - 1);
      auto it = styles.find(tag);
      if ((it != styles.end()) && (end < p_Html.size()) && ((p_Html[end] == '>') || (p_Html[end] == ' ')))
      {
        html += " style=\"" + it->second + "\"";
      }

      pos = end;
    }

    return html;
  }

  std::string HtmlToMarkdown(const std::string& p_Html, const HtmlOptions& p_Options)
  {
    Node root;
    if (!ParseHtml(p_Html, root)) return "";

    Walker walker(p_Options, true /*p_Markdown*/);
    return walker.Run(root);
  }

  std::string HtmlToText(const std::string& p_Html, const HtmlOptions& p_Options)
  {
    Node root;
    if (!ParseHtml(p_Html, root)) return "";

    Walker walker(p_Options, false /*p_Markdown*/);
    return walker.Run(root);
  }

  std::string MarkdownToHtml(const std::string& p_Markdown, const MarkdownOptions& p_Options)
  {
    unsigned flags = MD_DIALECT_GITHUB;
    if (p_Options.m_HardBreaks) flags |= MD_FLAG_HARD_SOFT_BREAKS;

    std::string body;
    const int rv = md_html(p_Markdown.c_str(), (MD_SIZE)p_Markdown.size(), OnHtmlOutput, &body, flags,
                           MD_HTML_FLAG_SKIP_UTF8_BOM);
    if (rv != 0) return "";

    static const std::string font =
      "font-family:-apple-system,'Segoe UI',Helvetica,Arial,sans-serif;font-size:14px;line-height:1.4;"
      "color:#222222";
    return "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n</head>\n<body>\n"
           "<div style=\"" + font + "\">\n" + AddStyles(body) + "</div>\n</body>\n</html>\n";
  }

  std::string GetVersion()
  {
    return "0.1.0";
  }
}
