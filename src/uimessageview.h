// uimessageview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "textstyle.h"
#include "uiscreenview.h"

class UiMessageView : public UiScreenView
{
public:
  explicit UiMessageView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();
  void Cleanup();

  bool CurrentMessageBodyHeaderAvailable();
  void ExportMessage();
  void ExtHtmlViewer();
  void ExtMsgViewer();
  std::string GetBodyText(Body& p_Body);

private:
  bool IsLinkSelected();
  void SelectLink(bool p_Next);
  void OpenLink();
  void SaveMessageView(bool p_Markdown);
  void MessageFind();
  void MessageFindNext();
  const std::vector<std::wstring>& GetCachedWordWrapLines(const std::string& p_Folder, uint32_t p_Uid);
  std::string GetHeaderLabel(const std::string& p_Name) const;

private:
  // message view header value column, labels as in compose, e.g. "Reply-To: "
  static const size_t HEADER_VALUE_COL = 10;
  // min header value width for wrapping with hanging indent
  static const size_t HEADER_VALUE_MIN_WIDTH = 30;

  bool m_AlignHeaderLabels = true;
  bool m_FullHeaderIncludeLocal = false;
  bool m_PersistFindQuery = false;
  bool m_PlainViewLinks = false;
  bool m_RespectFormatFlowed = true;
  bool m_RewrapQuotedLines = true;
  int m_TabSize = 8;
  bool m_MarkdownView = false;
  bool m_MarkdownViewLinks = false;
  bool m_ShowCompactHeader = false;

  std::string m_CurrentMessageViewText;
  bool m_CurrentMessageFromHtml = false;
  int m_MessageFindMatchPos = 0;
  int m_MessageFindMatchLen = 0;
  int m_MessageViewHeaderLineCount = 0;
  std::vector<std::wstring> m_MessageViewLines;
  std::vector<std::vector<TextStyle::Span>> m_MessageViewSpans;
  std::vector<TextStyle::Link> m_MessageViewLinks;
  std::pair<std::string, int32_t> m_MessageViewLinksFolderUid = std::make_pair("", -1);
  int m_MessageViewLinkIndex = -1;
  bool m_MessageViewLinkPastEnd = false; // index before first or after last link
  int m_MessageViewLinkPastEndOffset = 0; // view offset when past end
  bool m_ShowFullHeader = false;
};
