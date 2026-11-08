// walker.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nhtmlmd is distributed under the MIT license, see LICENSE for details.

#include "walker.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

#include "util.h"
#include "writer.h"

namespace Nhtmlmd
{
  bool IsSkipTag(const std::string& p_Tag)
  {
    static const std::set<std::string> tags =
    {
      "head", "style", "script", "title", "meta", "link", "noscript", "template",
      "svg", "object", "iframe", "button", "select", "input", "textarea",
    };
    return tags.count(p_Tag) > 0;
  }

  // 1 = block with blank line, 0 = block, -1 = inline
  static int BlockKind(const std::string& p_Tag)
  {
    static const std::map<std::string, int> tags =
    {
      { "p", 1 }, { "h1", 1 }, { "h2", 1 }, { "h3", 1 }, { "h4", 1 }, { "h5", 1 },
      { "h6", 1 }, { "pre", 1 }, { "dl", 1 }, { "figure", 1 }, { "hr", 1 },
      { "table", 1 }, { "blockquote", 1 },
      { "div", 0 }, { "center", 0 }, { "section", 0 }, { "article", 0 },
      { "header", 0 }, { "footer", 0 }, { "main", 0 }, { "nav", 0 }, { "aside", 0 },
      { "address", 0 }, { "dt", 0 }, { "dd", 0 }, { "figcaption", 0 }, { "form", 0 },
      { "fieldset", 0 }, { "details", 0 }, { "summary", 0 }, { "li", 0 },
      { "tr", 0 }, { "td", 0 }, { "th", 0 }, { "tbody", 0 }, { "thead", 0 },
      { "tfoot", 0 }, { "caption", 0 }, { "body", 0 }, { "html", 0 },
      { "ul", 0 }, { "ol", 0 },
    };
    auto it = tags.find(p_Tag);
    return (it != tags.end()) ? it->second : -1;
  }

  static bool IsHeading(const std::string& p_Tag)
  {
    return (p_Tag.size() == 2) && (p_Tag[0] == 'h') && (p_Tag[1] >= '1') && (p_Tag[1] <= '6');
  }

  // inline css hiding, incl. preheader tricks (max-height:0 etc)
  static bool IsHidden(const Node& p_Node)
  {
    if (p_Node.Attr("hidden") != nullptr) return true;

    const std::string* style = p_Node.Attr("style");
    if (style == nullptr) return false;

    const std::string css = Util::StripSpaces(*style);
    if ((css.find("display:none") != std::string::npos) ||
        (css.find("visibility:hidden") != std::string::npos) ||
        (css.find("mso-hide:all") != std::string::npos))
    {
      return true;
    }

    const bool overflowHidden = (css.find("overflow:hidden") != std::string::npos);
    for (const char* key : { "max-height:0", "height:0", "font-size:0", "width:0" })
    {
      const bool maxHeight = (strcmp(key, "max-height:0") == 0);
      const bool fontSize = (strcmp(key, "font-size:0") == 0);
      size_t pos = css.find(key);
      while (pos != std::string::npos)
      {
        const size_t end = pos + strlen(key);
        const bool zero = (end >= css.size()) || (css[end] == ';') || (css[end] == 'p') || (css[end] == '!');
        const bool prop = (pos == 0) || (css[pos - 1] == ';') || maxHeight;
        if (zero && prop && overflowHidden) return true;

        if (zero && prop && fontSize && (css.find("max-height") != std::string::npos)) return true;

        pos = css.find(key, pos + 1);
      }
    }

    return false;
  }

  static bool IsStruck(const Node& p_Node)
  {
    const std::string* style = p_Node.Attr("style");
    return (style != nullptr) && (Util::StripSpaces(*style).find("line-through") != std::string::npos);
  }

  static bool IsWebLink(const std::string* p_Href)
  {
    return (p_Href != nullptr) && ((p_Href->compare(0, 4, "http") == 0) || (p_Href->compare(0, 7, "mailto:") == 0));
  }

  // url without whitespace, as browsers do, to keep it one token
  static std::string CleanHref(const std::string& p_Href)
  {
    std::string href;
    for (char ch : p_Href)
    {
      if (ch == ' ')
      {
        href += "%20";
      }
      else if ((ch != '\t') && (ch != '\n') && (ch != '\r'))
      {
        href += ch;
      }
    }

    return href;
  }

  // css length in px, -1 if unknown
  static double CssLengthPx(const std::string& p_Value)
  {
    if (p_Value == "auto") return 0;

    char* end = nullptr;
    const double num = strtod(p_Value.c_str(), &end);
    if (end == p_Value.c_str()) return -1;

    const std::string unit(end);
    if (unit.empty() || (unit == "px")) return num;

    if (unit == "pt") return num * 4.0 / 3.0;

    if ((unit == "em") || (unit == "rem")) return num * 16.0;

    if (unit == "cm") return num * 37.8;

    if (unit == "mm") return num * 3.78;

    return -1;
  }

  // inline css vertical margin / padding in px, -1 if not set
  static void CssBox(const Node& p_Node, const std::string& p_Prop, double& p_Top, double& p_Bottom)
  {
    p_Top = -1;
    p_Bottom = -1;
    const std::string* style = p_Node.Attr("style");
    if (style == nullptr) return;

    const std::string css = Util::ToLower(*style);
    size_t start = 0;
    while (start < css.size())
    {
      size_t end = css.find(';', start);
      if (end == std::string::npos) end = css.size();

      const std::string decl = css.substr(start, end - start);
      start = end + 1;
      const size_t colon = decl.find(':');
      if (colon == std::string::npos) continue;

      const std::string key = Util::StripSpaces(decl.substr(0, colon));
      std::vector<std::string> values;
      std::string value;
      for (char ch : decl.substr(colon + 1) + " ")
      {
        if (isspace((unsigned char)ch) || (ch == '!'))
        {
          if (!value.empty() && (value != "important")) values.push_back(value);

          value.clear();
        }
        else
        {
          value += ch;
        }
      }

      if (values.empty()) continue;

      if (key == p_Prop)
      {
        p_Top = CssLengthPx(values[0]);
        p_Bottom = CssLengthPx((values.size() >= 3) ? values[2] : values[0]);
      }
      else if (key == (p_Prop + "-top"))
      {
        p_Top = CssLengthPx(values[0]);
      }
      else if (key == (p_Prop + "-bottom"))
      {
        p_Bottom = CssLengthPx(values[0]);
      }
    }
  }

  // blank lines from inline margins, < 10px none, >= 10px one
  static void MarginBlanks(const Node& p_Node, int& p_Before, int& p_After)
  {
    const std::string& tag = p_Node.m_Tag;
    const bool applies = (tag == "p") || (tag == "div") || (tag == "center") || (tag == "section") ||
      (tag == "article") || (tag == "ul") || (tag == "ol") || (tag == "dl") || (tag == "figure") ||
      IsHeading(tag);
    if (!applies) return;

    double top = -1;
    double bottom = -1;
    CssBox(p_Node, "margin", top, bottom);
    if (top >= 0) p_Before = (top >= 10) ? 1 : 0;

    if (bottom >= 0) p_After = (bottom >= 10) ? 1 : 0;
  }

  static bool HasText(const Node& p_Node)
  {
    for (const auto& child : p_Node.m_Children)
    {
      if (child->m_Type == Node::Text)
      {
        for (size_t i = 0; i < child->m_Text.size(); )
        {
          uint32_t cp = 0;
          i += Util::DecodeUtf8(child->m_Text, i, cp);
          if (!Util::IsSpace(cp) && !Util::IsJunk(cp)) return true;
        }
      }
      else if (!IsSkipTag(child->m_Tag) && !IsHidden(*child) && HasText(*child))
      {
        return true;
      }
    }

    return false;
  }

  static bool IsTextVisible(const std::string& p_Text)
  {
    for (size_t i = 0; i < p_Text.size(); )
    {
      uint32_t cp = 0;
      i += Util::DecodeUtf8(p_Text, i, cp);
      if (!Util::IsSpace(cp) && !Util::IsJunk(cp)) return true;
    }

    return false;
  }

  // edge text of subtree in one pass, as checking text per level would take quadratic time
  // for deeply nested elements, returns if subtree has text
  static bool EdgeSpaceSub(const Node& p_Node, bool p_Front, bool& p_Decided, bool& p_Space)
  {
    static const std::string nbsp = "\xC2\xA0";
    const auto& children = p_Node.m_Children;
    for (size_t i = 0; i < children.size(); ++i)
    {
      const Node& child = *children[p_Front ? i : (children.size() - 1 - i)];
      if (child.m_Type == Node::Text)
      {
        const std::string& text = child.m_Text;
        if (text.empty()) continue;

        if (!p_Decided)
        {
          const bool isNbsp = (text.size() >= 2) &&
            (p_Front ? (text.compare(0, 2, nbsp) == 0) : (text.compare(text.size() - 2, 2, nbsp) == 0));
          p_Space = isNbsp || isspace((unsigned char)(p_Front ? text.front() : text.back()));
          p_Decided = true;
        }

        if (IsTextVisible(text)) return true;
      }
      else if (!IsSkipTag(child.m_Tag) && !IsHidden(child))
      {
        // edge decided in child only if it has text
        bool childDecided = p_Decided;
        bool childSpace = p_Space;
        if (EdgeSpaceSub(child, p_Front, childDecided, childSpace))
        {
          p_Decided = childDecided;
          p_Space = childSpace;
          return true;
        }
      }
    }

    return false;
  }

  // subtree text starts (p_Front) or ends with whitespace
  static bool EdgeSpace(const Node& p_Node, bool p_Front)
  {
    bool decided = false;
    bool space = false;
    EdgeSpaceSub(p_Node, p_Front, decided, space);
    return decided && space;
  }

  static int Dimension(const Node& p_Node, const char* p_Name)
  {
    const std::string* attr = p_Node.Attr(p_Name);
    if ((attr != nullptr) && !attr->empty() && isdigit((unsigned char)(*attr)[0])) return atoi(attr->c_str());

    const std::string* style = p_Node.Attr("style");
    if (style != nullptr)
    {
      const std::string css = ";" + Util::StripSpaces(*style);
      const size_t pos = css.find(std::string(";") + p_Name + ":");
      if (pos != std::string::npos)
      {
        const size_t val = pos + strlen(p_Name) + 2;
        if ((val < css.size()) && isdigit((unsigned char)css[val])) return atoi(css.c_str() + val);
      }
    }

    return -1;
  }

  // alt text worth showing, i.e. not empty, cid, url or file name
  static std::string ImageAlt(const Node& p_Node)
  {
    const std::string* altAttr = p_Node.Attr("alt");
    if (altAttr == nullptr) return "";

    Writer writer;
    writer.Text(*altAttr);
    std::string alt = writer.Finish();
    while (!alt.empty() && (alt.back() == '\n'))
    {
      alt.pop_back();
    }

    if (alt.empty()) return "";

    const std::string lower = Util::ToLower(alt);
    if ((lower.compare(0, 4, "cid:") == 0) || (lower.compare(0, 4, "http") == 0) ||
        (lower.compare(0, 5, "data:") == 0) || (lower.find('/') != std::string::npos))
    {
      return "";
    }

    for (const char* ext : { ".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp" })
    {
      const size_t len = strlen(ext);
      if ((lower.size() > len) && (lower.compare(lower.size() - len, len, ext) == 0)) return "";
    }

    return alt;
  }

  static bool HasCodeChild(const Node& p_Node)
  {
    for (const auto& child : p_Node.m_Children)
    {
      if ((child->m_Type == Node::Element) && (child->m_Tag == "code")) return true;
    }

    return false;
  }

  // cell content is preformatted, e.g. a code / diff line
  static bool IsPreCell(const Node& p_Node)
  {
    for (const auto& child : p_Node.m_Children)
    {
      if (child->m_Type != Node::Element) continue;

      if ((child->m_Tag == "pre") || IsPreCell(*child)) return true;
    }

    return false;
  }

  static std::string CodeLanguage(const Node& p_Node)
  {
    for (const auto& child : p_Node.m_Children)
    {
      if ((child->m_Type != Node::Element) || (child->m_Tag != "code")) continue;

      const std::string* cls = child->Attr("class");
      if ((cls != nullptr) && (cls->compare(0, 9, "language-") == 0)) return cls->substr(9);
    }

    return "";
  }

  static void AppendText(const Node& p_Node, std::string& p_Text)
  {
    for (const auto& child : p_Node.m_Children)
    {
      if (child->m_Type == Node::Text)
      {
        p_Text += child->m_Text;
      }
      else
      {
        AppendText(*child, p_Text);
      }
    }
  }

  static bool IsMultiLine(const Node& p_Node)
  {
    std::string text;
    AppendText(p_Node, text);
    while (!text.empty() && (text.back() == '\n'))
    {
      text.pop_back();
    }

    return text.find('\n') != std::string::npos;
  }

  static void CollectRows(const Node& p_Node, std::vector<const Node*>& p_Rows)
  {
    for (const auto& child : p_Node.m_Children)
    {
      if (child->m_Type != Node::Element) continue;

      if (child->m_Tag == "tr")
      {
        p_Rows.push_back(child.get());
      }
      else if ((child->m_Tag == "tbody") || (child->m_Tag == "thead") || (child->m_Tag == "tfoot"))
      {
        CollectRows(*child, p_Rows);
      }
    }
  }

  static std::string EscapeCell(const std::string& p_Text)
  {
    std::string text;
    for (char ch : p_Text)
    {
      if (ch == '|') text += '\\';

      text += ch;
    }

    return text;
  }

  Walker::Walker(const HtmlOptions& p_Options, bool p_Markdown)
    : m_Options(p_Options)
    , m_Markdown(p_Markdown)
  {
  }

  std::string Walker::Run(const Node& p_Root)
  {
    Writer writer;
    m_Writer = &writer;
    Walk(p_Root);
    std::string str = writer.Finish();
    m_Writer = nullptr;

    if ((m_Options.m_LinkStyle == LinkStyleRefs) && !m_Links.empty())
    {
      str += m_Markdown ? "\n" : "\nReferences:\n";
      for (size_t i = 0; i < m_Links.size(); ++i)
      {
        str += "[" + std::to_string(i + 1) + (m_Markdown ? "]: " : "] ") + m_Links[i] + "\n";
      }
    }

    return str;
  }

  void Walker::Walk(const Node& p_Node)
  {
    if (p_Node.m_Type == Node::Text)
    {
      if (m_InPre > 0)
      {
        m_Writer->Pre(p_Node.m_Text);
      }
      else
      {
        m_Writer->Text(p_Node.m_Text);
      }

      return;
    }

    const std::string& tag = p_Node.m_Tag;
    if (IsSkipTag(tag) || IsHidden(p_Node)) return;

    // deeply nested inline styling walked as plain inline elements
    const bool styled = !IsRenderDeep();
    if (tag == "br")
    {
      m_Writer->LineBreak();
    }
    else if (tag == "img")
    {
      Image(p_Node);
    }
    else if (styled && ((tag == "del") || (tag == "s") || (tag == "strike") || IsStruck(p_Node)))
    {
      Emphasis(p_Node, "~~");
    }
    else if (styled && (tag == "sup"))
    {
      Superscript(p_Node);
    }
    else if (styled && m_Markdown && ((tag == "b") || (tag == "strong")))
    {
      Emphasis(p_Node, "**");
    }
    else if (styled && m_Markdown && ((tag == "i") || (tag == "em")))
    {
      Emphasis(p_Node, "*");
    }
    else if (styled && m_Markdown && ((tag == "code") || (tag == "kbd") || (tag == "samp")) && (m_InPre == 0))
    {
      Emphasis(p_Node, "`");
    }
    else if (m_Markdown && IsHeading(tag))
    {
      Heading(p_Node);
    }
    else if (tag == "hr")
    {
      m_Writer->Block(1);
      m_Writer->Literal(std::string(std::min(40, m_Options.m_LayoutWidth), '-'));
      m_Writer->Block(1);
    }
    else if (tag == "pre")
    {
      Pre(p_Node);
    }
    else if ((tag == "ul") || (tag == "ol"))
    {
      const int blank = m_Lists.empty() ? 1 : 0;
      m_Writer->Block(blank);
      List list;
      list.m_Ordered = (tag == "ol");
      const std::string* start = p_Node.Attr("start");
      if (start != nullptr) list.m_Next = atoi(start->c_str());

      m_Lists.push_back(list);
      WalkChildren(p_Node);
      m_Lists.pop_back();
      m_Writer->Block(blank);
    }
    else if (tag == "li")
    {
      m_Writer->Block(0);
      std::string marker = "- ";
      if (!m_Lists.empty() && m_Lists.back().m_Ordered)
      {
        marker = std::to_string(m_Lists.back().m_Next++) + ". ";
      }

      m_Writer->PushPrefix(marker, std::string(marker.size(), ' '));
      WalkChildren(p_Node);
      m_Writer->Block(0);
      m_Writer->PopPrefix();
    }
    else if (tag == "blockquote")
    {
      m_Writer->Block(1);
      m_Writer->PushPrefix("> ", "> ");
      WalkChildren(p_Node);
      m_Writer->Block(1);
      m_Writer->PopPrefix();
    }
    else if (tag == "table")
    {
      Table(p_Node);
    }
    else if (tag == "a")
    {
      Link(p_Node);
    }
    else
    {
      const int kind = BlockKind(tag);
      int before = kind;
      int after = kind;
      if (kind >= 0)
      {
        MarginBlanks(p_Node, before, after);
        m_Writer->Block(before);
      }

      WalkChildren(p_Node);
      if (kind >= 0)
      {
        m_Writer->Block(after);
      }
      else if ((tag == "td") || (tag == "th"))
      {
        m_Writer->Space();
      }
    }
  }

  void Walker::WalkChildren(const Node& p_Node)
  {
    for (const auto& child : p_Node.m_Children)
    {
      Walk(*child);
    }
  }

  void Walker::Heading(const Node& p_Node)
  {
    m_Writer->Block(1);
    m_Writer->PushPrefix(std::string(p_Node.m_Tag[1] - '0', '#') + " ", "");
    WalkChildren(p_Node);
    m_Writer->Block(1);
    m_Writer->PopPrefix();
  }

  void Walker::Pre(const Node& p_Node)
  {
    // fence code, but not one-line pre used for layout (diffs)
    const bool fence = m_Markdown && (HasCodeChild(p_Node) || IsMultiLine(p_Node));
    m_Writer->Block(1);
    if (fence) m_Writer->Literal("```" + CodeLanguage(p_Node));

    ++m_InPre;
    WalkChildren(p_Node);
    --m_InPre;
    if (fence) m_Writer->Literal("```");

    m_Writer->Block(1);
  }

  // inline marker around one-line text, edge spaces kept outside, multi-line text replayed
  // as rendered, as walking it again would take exponential time for nested elements
  void Walker::Emphasis(const Node& p_Node, const std::string& p_Mark)
  {
    std::vector<Writer::Op> ops;
    const std::string text = RenderSub(p_Node, nullptr, nullptr, &ops);
    if (text.empty()) return;

    if (text.find('\n') != std::string::npos)
    {
      m_Writer->Replay(ops);
      return;
    }

    if (EdgeSpace(p_Node, true)) m_Writer->Space();

    m_Writer->Text(p_Mark + text + p_Mark);
    if (EdgeSpace(p_Node, false)) m_Writer->Space();
  }

  // short superscript as ^x, except at line start, after digit or when long
  void Walker::Superscript(const Node& p_Node)
  {
    std::vector<Writer::Op> ops;
    const std::string text = RenderSub(p_Node, nullptr, nullptr, &ops);
    const bool isShort = !text.empty() && (Util::DisplayWidth(text) <= 4) &&
      (text.find_first_of(" \n") == std::string::npos);
    m_Writer->Superscript(text, isShort, std::move(ops));
  }

  void Walker::Image(const Node& p_Node)
  {
    const int width = Dimension(p_Node, "width");
    const int height = Dimension(p_Node, "height");
    if (((width >= 0) && (width <= 2)) || ((height >= 0) && (height <= 2))) return;

    const std::string alt = ImageAlt(p_Node);
    if (!m_LinkHasText.empty())
    {
      if (m_LinkHasText.back()) return;

      // skip linked avatars / icons without alt
      const std::string* src = p_Node.Attr("src");
      const bool cid = (src != nullptr) && (Util::ToLower(src->substr(0, 4)) == "cid:");
      const bool small = ((width >= 0) && (width <= 48)) || ((height >= 0) && (height <= 48));
      if (alt.empty() && (cid || small)) return;

      m_Writer->Space();
      m_Writer->Text("[" + (alt.empty() ? std::string("img") : alt) + "]");
      m_Writer->Space();
    }
    else if (!alt.empty())
    {
      m_Writer->Space();
      m_Writer->Text("[" + alt + "]");
      m_Writer->Space();
    }
  }

  void Walker::Link(const Node& p_Node)
  {
    // content rendered at most once, and replayed
    std::vector<Writer::Op> ops;
    bool rendered = false;
    bool empty = false;
    if (m_Markdown && (m_Options.m_LinkStyle != LinkStyleText) && MarkdownLink(p_Node, ops, rendered, empty)) return;

    const std::string* href = p_Node.Attr("href");
    const bool hasText = HasText(p_Node);
    m_LinkHasText.push_back(hasText);

    // no reference for links without content, like empty or skipped icon links
    const bool isWebRef = (m_Options.m_LinkStyle == LinkStyleRefs) && IsWebLink(href);
    if (isWebRef && !hasText && !rendered)
    {
      empty = RenderSub(p_Node, nullptr, nullptr, &ops).empty();
      rendered = true;
    }

    const bool isRef = isWebRef && (hasText || !empty);
    if (rendered)
    {
      m_Writer->Replay(ops);
    }
    else
    {
      WalkChildren(p_Node);
    }

    m_LinkHasText.pop_back();

    if (!IsWebLink(href)) return;

    if (isRef && m_Markdown)
    {
      // multi-line link, reference after it
      m_Writer->Space();
      m_Writer->Text("[link][" + std::to_string(LinkIndex(*href)) + "]");
    }
    else if (isRef)
    {
      m_Writer->Text("[" + std::to_string(LinkIndex(*href)) + "]");
    }
    else if ((m_Options.m_LinkStyle == LinkStyleInline) && HasText(p_Node))
    {
      const std::string text = RenderSub(p_Node);
      if ((text == *href) || (("mailto:" + text) == *href)) return;

      m_Writer->Space();
      m_Writer->Text("<" + CleanHref(*href) + ">");
    }
  }

  // one-line link with text as [text](url), [text][n] or <url>
  bool Walker::MarkdownLink(const Node& p_Node, std::vector<Writer::Op>& p_Ops, bool& p_Rendered,
                            bool& p_Empty)
  {
    const std::string* href = p_Node.Attr("href");
    if (!IsWebLink(href)) return false;

    const bool hasText = HasText(p_Node);
    m_LinkHasText.push_back(hasText);
    std::string text = RenderSub(p_Node, nullptr, nullptr, &p_Ops);
    m_LinkHasText.pop_back();
    p_Rendered = true;
    p_Empty = text.empty();

    // image only link with its [alt] as text
    if (!hasText)
    {
      if ((text.size() < 3) || (text.front() != '[') || (text.find(']') != (text.size() - 1))) return false;

      text = text.substr(1, text.size() - 2);
    }

    if (text.empty() || (text.find('\n') != std::string::npos)) return false;

    if (EdgeSpace(p_Node, true)) m_Writer->Space();

    if ((text == *href) || (("mailto:" + text) == *href))
    {
      m_Writer->Text("<" + text + ">");
    }
    else if (m_Options.m_LinkStyle == LinkStyleRefs)
    {
      m_Writer->Text("[" + text + "][" + std::to_string(LinkIndex(*href)) + "]");
    }
    else
    {
      m_Writer->Text("[" + text + "](" + CleanHref(*href) + ")");
    }

    if (EdgeSpace(p_Node, false)) m_Writer->Space();

    return true;
  }

  // nested inline styling beyond this is plain, to keep stack use low on threads
  bool Walker::IsRenderDeep() const
  {
    static const int maxRenderDepth = 32;
    return (m_RenderDepth >= maxRenderDepth);
  }

  size_t Walker::LinkIndex(const std::string& p_Href)
  {
    auto it = m_LinkIndex.find(p_Href);
    if (it != m_LinkIndex.end()) return it->second;

    m_Links.push_back(CleanHref(p_Href));
    m_LinkIndex[p_Href] = m_Links.size();
    return m_Links.size();
  }

  std::string Walker::RenderSub(const Node& p_Node, bool* p_BlankBefore, bool* p_BlankAfter,
                                std::vector<Writer::Op>* p_Ops)
  {
    Writer writer;
    writer.SetRecord(p_Ops != nullptr);
    Writer* parentWriter = m_Writer;
    m_Writer = &writer;
    ++m_RenderDepth;
    WalkChildren(p_Node);
    --m_RenderDepth;
    m_Writer = parentWriter;

    std::string str = writer.Finish();
    if (!str.empty() && (str.back() == '\n')) str.pop_back();

    if (p_BlankBefore != nullptr) *p_BlankBefore = writer.GetLeadingBlank();

    if (p_BlankAfter != nullptr) *p_BlankAfter = writer.GetTrailingBlank();

    if (p_Ops != nullptr) *p_Ops = writer.TakeOps();

    return str;
  }

  static size_t NonEmpty(const std::vector<std::string>& p_Texts)
  {
    return (size_t)std::count_if(p_Texts.begin(), p_Texts.end(),
                                 [](const std::string& p_Text) { return !p_Text.empty(); });
  }

  // render cells first, then choose grid or flattened layout from the result
  void Walker::Table(const Node& p_Node)
  {
    std::vector<const Node*> rows;
    CollectRows(p_Node, rows);

    std::vector<std::vector<Cell>> cells;
    size_t nonEmpty = 0;
    const Cell* onlyCell = nullptr;
    for (const Node* row : rows)
    {
      cells.emplace_back();
      for (const auto& child : row->m_Children)
      {
        if ((child->m_Type != Node::Element) || ((child->m_Tag != "td") && (child->m_Tag != "th"))) continue;

        if (IsHidden(*child)) continue;

        Cell cell;
        cell.m_Text = RenderSub(*child, &cell.m_BlankBefore, &cell.m_BlankAfter);
        cell.m_Header = (child->m_Tag == "th");
        cell.m_SingleLine = (cell.m_Text.find('\n') == std::string::npos);
        cell.m_Node = child.get();
        cells.back().push_back(cell);
      }
    }

    for (const auto& row : cells)
    {
      for (const auto& cell : row)
      {
        if (cell.m_Text.empty()) continue;

        ++nonEmpty;
        onlyCell = &cell;
      }
    }

    // single cell table is a wrapper, spaced by its padding / margin
    int before = 1;
    int after = 1;
    if (nonEmpty <= 1)
    {
      double padTop = -1;
      double padBottom = -1;
      double marTop = -1;
      double marBottom = -1;
      if (onlyCell != nullptr) CssBox(*onlyCell->m_Node, "padding", padTop, padBottom);

      CssBox(p_Node, "margin", marTop, marBottom);
      before = ((padTop >= 10) || (marTop >= 10)) ? 1 : 0;
      after = ((padBottom >= 10) || (marBottom >= 10)) ? 1 : 0;
    }

    m_Writer->Block(before);
    if (!GridTable(cells))
    {
      LayoutTable(cells);
    }

    m_Writer->Block(after);
  }

  void Walker::CellLines(const Cell& p_Cell, int p_Sep)
  {
    m_Writer->Block(p_Cell.m_BlankBefore ? 1 : p_Sep);
    for (const auto& line : Util::SplitLines(p_Cell.m_Text))
    {
      m_Writer->Literal(line);
    }

    m_Writer->Block(p_Cell.m_BlankAfter ? 1 : p_Sep);
  }

  void Walker::LayoutTable(const std::vector<std::vector<Cell>>& p_Cells)
  {
    for (const auto& row : p_Cells)
    {
      size_t count = 0;
      size_t multiLine = 0;
      size_t width = 0;
      bool before = false;
      bool after = false;
      for (const auto& cell : row)
      {
        if (cell.m_Text.empty()) continue;

        ++count;
        multiLine += cell.m_SingleLine ? 0 : 1;
        width += Util::DisplayWidth(cell.m_Text) + 3;
        before = before || cell.m_BlankBefore;
        after = after || cell.m_BlankAfter;
      }

      if (count == 0) continue;

      if ((count > 1) && (multiLine == 0) && (width <= (size_t)m_Options.m_LayoutWidth))
      {
        // row of one-line cells on one line
        std::string line;
        for (const auto& cell : row)
        {
          if (cell.m_Text.empty()) continue;

          line += (line.empty() ? "" : "   ") + cell.m_Text;
        }

        m_Writer->Block(before ? 1 : 0);
        m_Writer->Literal(line);
        m_Writer->Block(after ? 1 : 0);
      }
      else
      {
        // blank line between side-by-side cards
        const int sep = (multiLine >= 2) ? 1 : 0;
        for (const auto& cell : row)
        {
          if (cell.m_Text.empty()) continue;

          CellLines(cell, sep);
        }
      }
    }
  }

  // grid when >= 2 rows have >= 2 one-line cells and it fits layout width,
  // as markdown table when first grid row is a header row
  bool Walker::GridTable(const std::vector<std::vector<Cell>>& p_Cells)
  {
    static const size_t maxCellWidth = 60;
    bool markdown = false;
    for (const auto& row : p_Cells)
    {
      size_t count = 0;
      bool header = true;
      for (const auto& cell : row)
      {
        if (cell.m_Text.empty()) continue;

        ++count;
        header = header && cell.m_Header;
      }

      if (count < 2) continue;

      markdown = m_Markdown && header;
      break;
    }

    std::vector<std::vector<std::string>> texts;
    std::vector<bool> preCols;
    size_t gridRows = 0;
    size_t ncols = 0;
    for (const auto& row : p_Cells)
    {
      texts.emplace_back();
      for (const auto& cell : row)
      {
        texts.back().push_back(markdown ? EscapeCell(cell.m_Text) : cell.m_Text);
      }

      if (NonEmpty(texts.back()) < 2) continue;

      ++gridRows;
      ncols = std::max(ncols, row.size());
      preCols.resize(ncols, true);
      for (size_t col = 0; col < row.size(); ++col)
      {
        const Cell& cell = row[col];
        if (cell.m_Text.empty()) continue;

        if (!cell.m_SingleLine) return false;

        preCols[col] = preCols[col] && IsPreCell(*cell.m_Node);
      }
    }

    if (gridRows < 2) return false;

    std::vector<size_t> widths(ncols, 0);
    for (const auto& row : texts)
    {
      if (NonEmpty(row) < 2) continue;

      for (size_t col = 0; col < row.size(); ++col)
      {
        widths[col] = std::max(widths[col], Util::DisplayWidth(row[col]));
      }
    }

    // skip columns empty in all grid rows
    std::vector<size_t> keep;
    for (size_t col = 0; col < ncols; ++col)
    {
      if (widths[col] == 0) continue;

      keep.push_back(col);
    }

    if (keep.size() < 2) return false;

    // last column of code lines (diffs) may be wide, as nothing follows it
    const bool wideLast = !markdown && preCols[keep.back()];
    // code lines fenced in markdown, to not be parsed as markup
    const bool fence = m_Markdown && wideLast;
    size_t total = markdown ? 1 : 0;
    for (size_t i = 0; i < keep.size(); ++i)
    {
      if (wideLast && ((i + 1) == keep.size())) continue;

      if (widths[keep[i]] > maxCellWidth) return false;

      total += widths[keep[i]] + (markdown ? 3 : 2);
    }

    if (total > (size_t)m_Options.m_LayoutWidth) return false;

    // markdown tables restart after a single cell row, first row is header
    bool firstRow = true;
    bool inFence = false;
    for (size_t r = 0; r < p_Cells.size(); ++r)
    {
      const std::vector<std::string>& row = texts[r];
      const size_t count = NonEmpty(row);
      if (count == 0) continue;

      if (count == 1)
      {
        if (inFence)
        {
          m_Writer->Literal("```");
          m_Writer->Block(1);
          inFence = false;
        }

        for (const auto& cell : p_Cells[r])
        {
          if (!cell.m_Text.empty()) CellLines(cell, markdown ? 1 : 0);
        }

        firstRow = true;
        continue;
      }

      std::string line = markdown ? "| " : "";
      bool header = true;
      for (size_t i = 0; i < keep.size(); ++i)
      {
        const size_t col = keep[i];
        const std::string text = (col < row.size()) ? row[col] : std::string();
        if (!text.empty()) header = header && p_Cells[r][col].m_Header;

        const bool last = ((i + 1) == keep.size());
        const size_t pad = widths[col] - Util::DisplayWidth(text);
        if (markdown)
        {
          line += text + std::string(pad, ' ') + (last ? " |" : " | ");
        }
        else
        {
          line += text + (last ? "" : std::string(pad + 2, ' '));
        }
      }

      if (firstRow && markdown) m_Writer->Block(1);

      if (fence && !inFence)
      {
        m_Writer->Block(1);
        m_Writer->Literal("```");
        inFence = true;
      }

      m_Writer->Block(0);
      m_Writer->Literal(line);
      if (firstRow && (markdown || header))
      {
        std::string sep = markdown ? "|" : "";
        for (size_t i = 0; i < keep.size(); ++i)
        {
          const bool last = ((i + 1) == keep.size());
          if (markdown)
          {
            sep += std::string(widths[keep[i]] + 2, '-') + "|";
          }
          else
          {
            sep += std::string(widths[keep[i]], '-') + (last ? "" : "  ");
          }
        }

        m_Writer->Literal(sep);
      }

      firstRow = false;
    }

    if (inFence)
    {
      m_Writer->Literal("```");
      m_Writer->Block(1);
    }

    return true;
  }
}
