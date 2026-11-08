// nhtmlmd.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

namespace Nhtmlmd
{
  enum LinkStyle
  {
    LinkStyleText = 0,  // link text only
    LinkStyleRefs,      // [text][1] / text[1] with references listed at end
    LinkStyleInline,    // [text](url) / text <url>
  };

  struct HtmlOptions
  {
    LinkStyle m_LinkStyle = LinkStyleText;
    int m_LayoutWidth = 80;  // max width of content laid out by nhtmlmd (tables, separators)
  };

  struct MarkdownOptions
  {
    bool m_HardBreaks = false;  // every newline is a line break
  };

  // html (utf-8) to markdown
  std::string HtmlToMarkdown(const std::string& p_Html, const HtmlOptions& p_Options = HtmlOptions());

  // html (utf-8) to plain text
  std::string HtmlToText(const std::string& p_Html, const HtmlOptions& p_Options = HtmlOptions());

  // markdown (github flavored, utf-8) to complete html document
  std::string MarkdownToHtml(const std::string& p_Markdown,
                             const MarkdownOptions& p_Options = MarkdownOptions());

  std::string GetVersion();
}
