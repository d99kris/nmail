// uicomposeview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uicomposeview.h"

#include <algorithm>
#include <cstdint>

#include <unistd.h>

#include "apputil.h"
#include "extutil.h"
#include "fileutil.h"
#include "loghelp.h"
#include "mailutil.h"
#include "offlinequeue.h"
#include "strutil.h"
#include "textutil.h"
#include "textwrap.h"
#include "uiconfig.h"
#include "uiexternal.h"
#include "uimessagelistview.h"
#include "uimessageview.h"
#include "uimodel.h"

UiComposeView::UiComposeView(Ui* p_Ui)
  : UiScreenView(p_Ui)
{
  m_TabSize = StrUtil::Bound(1, UiConfig::GetNum("tab_size"), 80);
  m_RewrapQuotedLines = UiConfig::GetBool("rewrap_quoted_lines");
  m_RespectFormatFlowed = UiConfig::GetBool("respect_format_flowed");
  m_ComposeLineWrap = UiConfig::GetNum("compose_line_wrap");
  m_CancelWithoutConfirm = UiConfig::GetBool("cancel_without_confirm");
  m_PostponeWithoutConfirm = UiConfig::GetBool("postpone_without_confirm");
  m_SendWithoutConfirm = UiConfig::GetBool("send_without_confirm");
  m_Signature = UiConfig::GetBool("signature");
  m_BottomReply = UiConfig::GetBool("bottom_reply");
  m_MarkdownHtmlCompose = UiConfig::GetBool("markdown_html_compose");
  m_UnwrapQuotedLines = UiConfig::GetBool("unwrap_quoted_lines");
  m_ShowRichHeader = UiConfig::GetBool("show_rich_header");

  try
  {
    m_ComposeBackupInterval = std::stoll(UiConfig::GetStr("compose_backup_interval_sec"));
  }
  catch (...)
  {
  }
}

void UiComposeView::Cleanup()
{
  UiConfig::SetBool("show_rich_header", m_ShowRichHeader);
}

void UiComposeView::Draw()
{
  const bool processFlowed = false; // only process when viewing message
  const bool outputFlowed = false; // only generate when sending after compose
  const bool quoteWrap = false; // only wrap quoted lines when viewing message
  const int expandTabSize = 0; // disabled
  m_ComposeMessageLines = TextUtil::WordWrap(m_ComposeMessageStr, m_MaxComposeLineLength,
                                             processFlowed, outputFlowed, quoteWrap, expandTabSize,
                                             m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                             m_ComposeMessageWrapPos, &m_ComposeMessageLineOffsets);

  std::vector<std::wstring> headerLines;
  if (m_ShowRichHeader)
  {
    headerLines =
    {
      L"From    : ",
      L"To      : ",
      L"Cc      : ",
      L"Bcc     : ",
      L"Attchmnt: ",
      L"Subject : ",
    };
  }
  else
  {
    headerLines =
    {
      L"To      : ",
      L"Cc      : ",
      L"Attchmnt: ",
      L"Subject : ",
    };
  }

  int cursY = 0;
  int cursX = 0;
  if (m_IsComposeHeader)
  {
    m_ComposeHeaderLine = std::min(m_ComposeHeaderLine, (int)m_ComposeHeaderStr.size() - 1);
    cursY = m_ComposeHeaderLine;
  }
  else
  {
    cursY = headerLines.size() + 1 + m_ComposeMessageWrapLine;
    cursX = GetComposeMessageColumn();
  }

  werase(m_MainWin);

  std::vector<std::wstring> composeLines;

  for (int i = 0; i < (int)headerLines.size(); ++i)
  {
    const std::wstring& headerLabel = headerLines.at(i);
    std::wstring headerStr = m_ComposeHeaderStr.at(i);
    if (m_IsComposeHeader && (i == m_ComposeHeaderLine))
    {
      // scroll field horizontally, based on display width, to keep cursor within screen
      const int labelWidth = TextWrap::GetWidth(headerLabel);
      const int maxWidth = std::max(m_ScreenWidth - 1 - labelWidth, 0);
      const size_t headerPos = StrUtil::Bound(0, m_ComposeHeaderPos, (int)headerStr.size());
      size_t startPos = 0;
      int width = TextWrap::GetWidth(headerStr.substr(0, headerPos));
      while ((width > maxWidth) && (startPos < headerPos))
      {
        const size_t endPos = TextWrap::GetClusterEnd(headerStr, startPos);
        width -= TextWrap::GetClusterWidth(headerStr, startPos, endPos);
        startPos = endPos;
      }

      headerStr = headerStr.substr(startPos);
      cursX = labelWidth + std::max(width, 0);
    }

    composeLines.push_back(StrUtil::TrimWString(headerLabel + headerStr, m_ScreenWidth));
  }

  composeLines.push_back(L"");

  for (auto line = m_ComposeMessageLines.begin(); line != m_ComposeMessageLines.end(); ++line)
  {
    composeLines.push_back(*line);
  }

  if (cursY < m_ComposeMessageOffsetY)
  {
    m_ComposeMessageOffsetY = std::max(m_ComposeMessageOffsetY - (m_MainWinHeight / 2), 0);

  }
  else if (cursY >= (m_ComposeMessageOffsetY + m_MainWinHeight))
  {
    m_ComposeMessageOffsetY += (m_MainWinHeight / 2);
  }

  int messageY = 0;
  int idx = 0;
  for (auto line = composeLines.begin(); line != composeLines.end(); ++line, ++idx)
  {
    if (idx < m_ComposeMessageOffsetY) continue;

    if (messageY > m_MainWinHeight) break;

    if (idx < (int)headerLines.size())
    {
      // header name incl colon, and value
      const size_t nameLen = std::min(headerLines.at(idx).find(L':') + 1, line->size());
      const std::string& name = StrUtil::ToString(line->substr(0, nameLen));
      const std::string& value = StrUtil::ToString(line->substr(nameLen));
      wattron(m_MainWin, m_AttrsHeaderName);
      mvwprintw(m_MainWin, messageY, 0, "%s", name.c_str());
      wattroff(m_MainWin, m_AttrsHeaderName);
      wprintw(m_MainWin, "%s", value.c_str());
      ++messageY;
      continue;
    }

    const std::string& dispStr = StrUtil::ToString(*line);
    const bool isQuote = (dispStr.rfind(">", 0) == 0);

    if (isQuote)
    {
      wattron(m_MainWin, m_AttrsQuotedText);
    }

    mvwprintw(m_MainWin, messageY, 0, "%s", dispStr.c_str());

    if (isQuote)
    {
      wattroff(m_MainWin, m_AttrsQuotedText);
    }

    ++messageY;
  }

  cursY -= m_ComposeMessageOffsetY;

  leaveok(m_MainWin, false);
  wmove(m_MainWin, cursY, cursX);
  RefreshWin(m_MainWin);
  leaveok(m_MainWin, true);
}

void UiComposeView::KeyHandler(int p_Key)
{
  bool continueProcess = false;
  bool asyncRedraw = false;

  // process state-specific key handling first
  if (m_IsComposeHeader)
  {
    if (p_Key == m_KeyUp)
    {
      --m_ComposeHeaderLine;
      if (m_ComposeHeaderLine < 0)
      {
        m_ComposeHeaderPos = 0;
      }

      m_ComposeHeaderLine = StrUtil::Bound(0, m_ComposeHeaderLine,
                                           (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = StrUtil::Bound(0, m_ComposeHeaderPos,
                                          (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == m_KeyDown) || (p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) || (p_Key == m_KeyTab))
    {
      if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
      {
        m_ComposeHeaderLine = StrUtil::Bound(0, m_ComposeHeaderLine + 1,
                                             (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = StrUtil::Bound(0, m_ComposeHeaderPos,
                                            (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_IsComposeHeader = false;
      }
    }
    else if (p_Key == m_KeyPrevPageCompose)
    {
      m_ComposeHeaderLine = 0;
      m_ComposeHeaderPos = 0;
    }
    else if (p_Key == m_KeyNextPageCompose)
    {
      m_IsComposeHeader = false;
    }
    else if (p_Key == m_KeyToSelect)
    {
      int headerField = GetCurrentHeaderField();
      if ((headerField == Ui::HeaderTo) || (headerField == Ui::HeaderCc) || (headerField == Ui::HeaderBcc))
      {
        SetState(Ui::StateAddressList);
      }
      else if (headerField == Ui::HeaderFrom)
      {
        SetState(Ui::StateFromAddressList);
      }
      else if (headerField == Ui::HeaderAtt)
      {
        FilePickerOrStateFileList();
      }
    }
    else if ((p_Key == m_KeyLeft) && (m_ComposeHeaderPos == 0))
    {
      --m_ComposeHeaderLine;
      if (m_ComposeHeaderLine < 0)
      {
        m_ComposeHeaderPos = 0;
      }
      else
      {
        m_ComposeHeaderPos = std::numeric_limits<int>::max();
      }

      m_ComposeHeaderLine = StrUtil::Bound(0, m_ComposeHeaderLine,
                                           (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = StrUtil::Bound(0, m_ComposeHeaderPos,
                                          (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == m_KeyRight) && (m_ComposeHeaderPos == (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size()))
    {
      m_ComposeHeaderPos = 0;

      if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
      {
        m_ComposeHeaderLine = StrUtil::Bound(0, m_ComposeHeaderLine + 1,
                                             (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = StrUtil::Bound(0, m_ComposeHeaderPos,
                                            (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_IsComposeHeader = false;
        m_ComposeMessagePos = 0;
      }
    }
    else if (HandleComposeKey(p_Key))
    {
      // none
    }
    else if (HandleLineKey(p_Key, m_ComposeHeaderStr[m_ComposeHeaderLine], m_ComposeHeaderPos))
    {
      // none
    }
    else if (HandleDocKey(p_Key, m_ComposeHeaderStr[m_ComposeHeaderLine], m_ComposeHeaderPos))
    {
      // none
    }
    else
    {
      continueProcess = true;
    }
  }
  else // compose body
  {
    if (p_Key == m_KeyUp)
    {
      ComposeMessagePrevLine();
    }
    else if (p_Key == m_KeyDown)
    {
      ComposeMessageNextLine();
    }
    else if (p_Key == m_KeyPrevPageCompose)
    {
      const bool processFlowed = false; // only process when viewing message
      const bool outputFlowed = false; // only generate when sending after compose
      const bool quoteWrap = false; // only wrap quoted lines when viewing message
      const int expandTabSize = 0; // disabled
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessagePrevLine();
        m_ComposeMessageLines = TextUtil::WordWrap(m_ComposeMessageStr, m_MaxComposeLineLength,
                                                   processFlowed, outputFlowed, quoteWrap, expandTabSize,
                                                   m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                                   m_ComposeMessageWrapPos, &m_ComposeMessageLineOffsets);
      }
    }
    else if (p_Key == m_KeyNextPageCompose)
    {
      const bool processFlowed = false; // only process when viewing message
      const bool outputFlowed = false; // only generate when sending after compose
      const bool quoteWrap = false; // only wrap quoted lines when viewing message
      const int expandTabSize = 0; // disabled
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessageNextLine();
        m_ComposeMessageLines = TextUtil::WordWrap(m_ComposeMessageStr, m_MaxComposeLineLength,
                                                   processFlowed, outputFlowed, quoteWrap, expandTabSize,
                                                   m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                                   m_ComposeMessageWrapPos, &m_ComposeMessageLineOffsets);
      }
    }
    else if ((p_Key == m_KeyLeft) && (m_ComposeMessagePos == 0))
    {
      // switch to header
      m_IsComposeHeader = true;
      m_ComposeHeaderPos = (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size();
    }
    else if (p_Key == m_KeyTab)
    {
      DrawAll(); // redraw to update current column position (m_ComposeMessageWrapPos)
      int tabSpaces = (m_TabSize - (GetComposeMessageColumn() % m_TabSize));
      for (int i = 0; i < tabSpaces; ++i)
      {
        m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, ' ');
      }

      asyncRedraw = true;
    }
    else if (HandleComposeKey(p_Key))
    {
      // none
    }
    else if (HandleLineKey(p_Key, m_ComposeMessageStr, m_ComposeMessagePos))
    {
      // none
    }
    else if (HandleDocKey(p_Key, m_ComposeMessageStr, m_ComposeMessagePos))
    {
      // none
    }
    else
    {
      continueProcess = true;
    }
  }

  // process common key handling last (if not handled above)
  if (continueProcess)
  {
    if (IsValidTextKey(p_Key))
    {
      if (m_IsComposeHeader)
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos++, 1, p_Key);
      }
      else
      {
        m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, p_Key);
      }

      asyncRedraw = true;
    }
    else if (m_InvalidInputNotify)
    {
      SetDialogMessage("Invalid input (" + StrUtil::ToHexString(p_Key) + ")");
    }
  }

  if (asyncRedraw)
  {
    AsyncUiRequest(Ui::UiRequestDrawAll);
  }
  else
  {
    DrawAll();
  }
}

bool UiComposeView::ComposedMessageIsValid(bool p_ForSend)
{
  std::string addrs =
    StrUtil::ToString(GetComposeStr(Ui::HeaderTo)) +
    StrUtil::ToString(GetComposeStr(Ui::HeaderCc)) +
    StrUtil::ToString(GetComposeStr(Ui::HeaderBcc));
  if (p_ForSend && (addrs.find("@") == std::string::npos))
  {
    SetDialogMessage("No recipients specified");
    return false;
  }

  std::vector<std::string> atts = FileUtil::SplitPaths(StrUtil::ToString(GetComposeStr(Ui::HeaderAtt)));
  for (auto& att : atts)
  {
    if (!FileUtil::IsReadableFile(att))
    {
      std::string path = att;
      int maxPath = (m_ScreenWidth - 26);
      if ((int)path.size() > maxPath)
      {
        int offset = path.size() - maxPath;
        path = "..." + path.substr(offset);
      }

      LOG_WARNING("file \"%s\" not found", att.c_str());
      SetDialogMessage("File \"" + path + "\" not found");
      return false;
    }
  }

  std::string subject = StrUtil::ToString(GetComposeStr(Ui::HeaderSub));
  if (p_ForSend && subject.empty())
  {
    if (!PromptYesNo("No subject specified, continue (y/n)?"))
    {
      return false;
    }
  }

  return true;
}

void UiComposeView::SendComposedMessage()
{
  SmtpManager::Action smtpAction;
  smtpAction.m_From = StrUtil::ToString(GetComposeStr(Ui::HeaderFrom));
  smtpAction.m_To = StrUtil::ToString(GetComposeStr(Ui::HeaderTo));
  smtpAction.m_Cc = StrUtil::ToString(GetComposeStr(Ui::HeaderCc));
  smtpAction.m_Bcc = StrUtil::ToString(GetComposeStr(Ui::HeaderBcc));
  smtpAction.m_Att = StrUtil::ToString(GetComposeStr(Ui::HeaderAtt));
  smtpAction.m_Subject = StrUtil::ToString(GetComposeStr(Ui::HeaderSub));
  smtpAction.m_Body = StrUtil::ToString(GetComposeBodyForSend());
  smtpAction.m_HtmlBody = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
  smtpAction.m_RefMsgId = m_ComposeHeaderRef;
  smtpAction.m_ComposeTempDirectory = m_ComposeTempDirectory;
  smtpAction.m_ComposeDraftUid = m_ComposeDraftUid;
  smtpAction.m_FormatFlowed = (m_ComposeLineWrap == Ui::LineWrapFormatFlowed);

  if (IsConnected())
  {
    smtpAction.m_IsSendMessage = true;
    m_SmtpManager->AsyncAction(smtpAction);
  }
  else
  {
    smtpAction.m_IsCreateMessage = true;
    SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
    if (smtpResult.m_SmtpStatus == SmtpStatusOk)
    {
      OfflineQueue::PushOutboxMessage(smtpResult.m_Message);
      SetDialogMessage("Message queued for sending");
    }
    else
    {
      SetDialogMessage("Message creation failed", true /* p_Warn */);
    }
  }
}

void UiComposeView::UploadDraftMessage()
{
  const std::string draftsFolder = m_Model->GetDraftsFolder();
  if (!draftsFolder.empty())
  {
    SmtpManager::Action smtpAction;
    smtpAction.m_IsCreateMessage = true;
    smtpAction.m_From = StrUtil::ToString(GetComposeStr(Ui::HeaderFrom));
    smtpAction.m_To = StrUtil::ToString(GetComposeStr(Ui::HeaderTo));
    smtpAction.m_Cc = StrUtil::ToString(GetComposeStr(Ui::HeaderCc));
    smtpAction.m_Bcc = StrUtil::ToString(GetComposeStr(Ui::HeaderBcc));
    smtpAction.m_Att = StrUtil::ToString(GetComposeStr(Ui::HeaderAtt));
    smtpAction.m_Subject = StrUtil::ToString(GetComposeStr(Ui::HeaderSub));
    smtpAction.m_Body = StrUtil::ToString(GetComposeBodyForSend());
    smtpAction.m_HtmlBody = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
    smtpAction.m_RefMsgId = m_ComposeHeaderRef;

    SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
    if (smtpResult.m_SmtpStatus == SmtpStatusOk)
    {
      ImapManager::Action imapAction;
      imapAction.m_UploadDraft = true;
      imapAction.m_Folder = draftsFolder;
      imapAction.m_Msg = smtpResult.m_Message;

      if (IsConnected())
      {
        m_Model->GetImapManager()->AsyncAction(imapAction);

        if (m_ComposeDraftUid != 0)
        {
          m_Model->MoveMessages(std::set<uint32_t>({ m_ComposeDraftUid }), draftsFolder, m_Model->GetTrashFolder());
          m_MessageListView->SyncCursor();
        }

        m_Model->SetHasRequestedUids(draftsFolder, false);
      }
      else
      {
        OfflineQueue::PushDraftMessage(smtpResult.m_Message);
        SetDialogMessage("Message queued for draft upload");
      }
    }
    else
    {
      SetDialogMessage("Message creation failed", true /* p_Warn */);
    }
  }
  else
  {
    SetDialogMessage("Drafts folder not configured", true /* p_Warn */);
  }
}

int UiComposeView::GetComposeMessageColumn()
{
  // display column of current position, which differs from position in line for wide characters
  if (m_ComposeMessageWrapLine >= (int)m_ComposeMessageLines.size()) return 0;

  const std::wstring& line = m_ComposeMessageLines[m_ComposeMessageWrapLine];
  return TextWrap::GetWidth(line.substr(0, m_ComposeMessageWrapPos));
}

void UiComposeView::SetComposeMessagePos(int p_WrapLine, int p_Column)
{
  // set position to specified line and display column, or end of line if shorter
  if ((p_WrapLine < (int)m_ComposeMessageLines.size()) &&
      (p_WrapLine < (int)m_ComposeMessageLineOffsets.size()))
  {
    const std::wstring& line = m_ComposeMessageLines[p_WrapLine];
    m_ComposeMessagePos = m_ComposeMessageLineOffsets[p_WrapLine] + (int)TextWrap::GetPosAtColumn(line, p_Column);
  }
  else
  {
    m_ComposeMessagePos = (int)m_ComposeMessageStr.size();
  }

  m_ComposeMessagePos = StrUtil::Bound(0, m_ComposeMessagePos, (int)m_ComposeMessageStr.size());
}

void UiComposeView::ComposeMessagePrevLine()
{
  if (m_ComposeMessageWrapLine > 0)
  {
    SetComposeMessagePos(m_ComposeMessageWrapLine - 1, GetComposeMessageColumn());
  }
  else
  {
    m_IsComposeHeader = true;
  }
}

void UiComposeView::ComposeMessageNextLine()
{
  if (m_ComposeMessagePos < (int)m_ComposeMessageStr.size())
  {
    SetComposeMessagePos(m_ComposeMessageWrapLine + 1, GetComposeMessageColumn());
  }
}

// compose fields from mailto:addrs?cc=..&bcc=..&subject=..&body=.. (rfc 6068)
void UiComposeView::SetComposeMailto(const std::string& p_Url)
{
  // header values without ascii control chars, like newlines, keeping utf-8 bytes
  auto decodeHeader = [](const std::string& p_Str)
  {
    std::string str = MailUtil::DecodeUrl(p_Str);
    str.erase(std::remove_if(str.begin(), str.end(),
                             [](char p_Ch) { return ((unsigned char)p_Ch < 0x20) || (p_Ch == 0x7f); }),
              str.end());
    return str;
  };

  const std::string mailto = p_Url.substr(7);
  const size_t queryPos = mailto.find('?');
  std::vector<std::string> tos;
  std::vector<std::string> ccs;
  std::vector<std::string> bccs;
  std::string subject;
  std::string body;
  const std::string addrs = mailto.substr(0, queryPos);
  if (!addrs.empty())
  {
    tos.push_back(decodeHeader(addrs));
  }

  if (queryPos != std::string::npos)
  {
    for (const auto& field : StrUtil::Split(mailto.substr(queryPos + 1), '&'))
    {
      const size_t eqPos = field.find('=');
      if (eqPos == std::string::npos) continue;

      const std::string name = StrUtil::ToLower(field.substr(0, eqPos));
      const std::string value = field.substr(eqPos + 1);
      if (name == "to")
      {
        tos.push_back(decodeHeader(value));
      }
      else if (name == "cc")
      {
        ccs.push_back(decodeHeader(value));
      }
      else if (name == "bcc")
      {
        bccs.push_back(decodeHeader(value));
      }
      else if (name == "subject")
      {
        subject = decodeHeader(value);
      }
      else if (name == "body")
      {
        body = MailUtil::DecodeUrl(value);
      }
    }
  }

  // bcc only in rich header
  if (!bccs.empty() && !m_ShowRichHeader)
  {
    m_ShowRichHeader = true;
    SetComposeStr(Ui::HeaderAll, L"");
    SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    m_ComposeHeaderLine = 1;
  }

  SetComposeStr(Ui::HeaderTo, StrUtil::ToWString(StrUtil::Join(tos, ", ")));
  SetComposeStr(Ui::HeaderCc, StrUtil::ToWString(StrUtil::Join(ccs, ", ")));
  SetComposeStr(Ui::HeaderBcc, StrUtil::ToWString(StrUtil::Join(bccs, ", ")));
  SetComposeStr(Ui::HeaderSub, StrUtil::ToWString(subject));
  if (!body.empty())
  {
    std::wstring wbody = StrUtil::ToWString(body);
    StrUtil::StripCR(wbody);
    m_ComposeMessageStr = wbody + L"\n" + m_ComposeMessageStr;
  }

  // cursor at first field to fill in
  if (subject.empty())
  {
    m_ComposeHeaderLine = m_ShowRichHeader ? 5 : 3;
  }
  else
  {
    m_IsComposeHeader = false;
  }
}

std::wstring UiComposeView::GetComposeStr(int p_HeaderField)
{
  if (m_ShowRichHeader)
  {
    return m_ComposeHeaderStr[p_HeaderField];
  }
  else
  {
    switch (p_HeaderField)
    {
      case Ui::HeaderTo: return m_ComposeHeaderStr[0];

      case Ui::HeaderCc: return m_ComposeHeaderStr[1];

      case Ui::HeaderAtt: return m_ComposeHeaderStr[2];

      case Ui::HeaderSub: return m_ComposeHeaderStr[3];

      default: break;
    }
  }

  return L"";
}

void UiComposeView::SetComposeStr(int p_HeaderField, const std::wstring& p_Str)
{
  if (p_HeaderField == Ui::HeaderAll)
  {
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = p_Str;
    m_ComposeHeaderStr[1] = p_Str;
    m_ComposeHeaderStr[2] = p_Str;
    m_ComposeHeaderStr[3] = p_Str;
    if (m_ShowRichHeader)
    {
      m_ComposeHeaderStr[4] = p_Str;
      m_ComposeHeaderStr[5] = p_Str;
    }
  }
  else
  {
    if (m_ShowRichHeader)
    {
      m_ComposeHeaderStr[p_HeaderField] = p_Str;
    }
    else
    {
      switch (p_HeaderField)
      {
        case Ui::HeaderTo:
          m_ComposeHeaderStr[0] = p_Str;
          break;

        case Ui::HeaderCc:
          m_ComposeHeaderStr[1] = p_Str;
          break;

        case Ui::HeaderAtt:
          m_ComposeHeaderStr[2] = p_Str;
          break;

        case Ui::HeaderSub:
          m_ComposeHeaderStr[3] = p_Str;
          break;

        default:
          break;
      }
    }
  }
}

std::wstring UiComposeView::GetComposeBodyForSend()
{
  if (m_ComposeLineWrap == Ui::LineWrapNone)
  {
    return m_ComposeMessageStr;
  }
  else if (m_ComposeLineWrap == Ui::LineWrapFormatFlowed)
  {
    const bool processFlowed = m_CurrentMessageProcessFlowed;
    const bool outputFlowed = true;
    const bool quoteWrap = m_RewrapQuotedLines;
    const int expandTabSize = 0; // disabled
    std::vector<std::wstring> indentBodyLines =
      TextUtil::WordWrap(m_ComposeMessageStr, 72, processFlowed, outputFlowed, quoteWrap, expandTabSize);
    return StrUtil::Join(indentBodyLines);
  }
  else if (m_ComposeLineWrap == Ui::LineWrapHardWrap)
  {
    return StrUtil::Join(m_ComposeMessageLines);
  }
  else
  {
    LOG_WARNING("invalid line wrap %d", m_ComposeLineWrap);
  }
  return L"";
}

int UiComposeView::GetCurrentHeaderField()
{
  if (m_ShowRichHeader)
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return Ui::HeaderFrom;

      case 1: return Ui::HeaderTo;

      case 2: return Ui::HeaderCc;

      case 3: return Ui::HeaderBcc;

      case 4: return Ui::HeaderAtt;

      case 5: return Ui::HeaderSub;

      default: break;
    }
  }
  else
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return Ui::HeaderTo;

      case 1: return Ui::HeaderCc;

      case 2: return Ui::HeaderAtt;

      case 3: return Ui::HeaderSub;

      default: break;
    }
  }

  return Ui::HeaderAll;
}

std::string UiComposeView::MakeHtmlPart(const std::string& p_Text)
{
  if (!m_CurrentMarkdownHtmlCompose) return std::string();

  if (m_Signature)
  {
    std::string htmlPartCustomSignature = MakeHtmlPartCustomSig(p_Text);
    if (!htmlPartCustomSignature.empty())
    {
      return htmlPartCustomSignature;
    }
  }

  return ExtUtil::ConvertTextToHtml(p_Text);
}

std::string UiComposeView::MakeHtmlPartCustomSig(const std::string& p_Text)
{
  std::string signatureStr = StrUtil::ToString(GetSignatureStr(true /* p_NoPrefix */));
  static const std::string sigHtmlPath = AppUtil::GetApplicationDir() + "signature.html";
  if (!FileUtil::IsReadableFile(sigHtmlPath))
  {
    return std::string();
  }

  std::string signatureHtmlStr = FileUtil::ReadFile(sigHtmlPath);
  if (signatureHtmlStr.empty())
  {
    LOG_WARNING("html signature file is empty");
    return std::string();
  }

  char randhex[80];
  snprintf(randhex, sizeof(randhex), "%lx.%lx.%x.%lx",
           (long)time(NULL), random(), getpid(), random());
  const std::string placeholderStr = std::string(randhex);

  std::string tmpText = p_Text;
  if (!StrUtil::ReplaceStringFirst(tmpText, signatureStr, "  \n" + placeholderStr))
  {
    LOG_WARNING("plain text signature not found in message, cannot use custom html signature");
    return std::string();
  }

  tmpText = ExtUtil::ConvertTextToHtml(tmpText);
  const std::string placeholderHtmlStr = "<p>" + placeholderStr + "</p>";
  if (StrUtil::ReplaceStringCount(tmpText, placeholderHtmlStr, signatureHtmlStr) != 1)
  {
    LOG_WARNING("unique custom html signature placeholder not found");
    return std::string();
  }

  return tmpText;
}

void UiComposeView::StartComposeBackup()
{
  if (m_ComposeBackupInterval != 0)
  {
    m_ComposeBackupRunning = true;
    m_ComposeBackupThread = std::thread(&UiComposeView::ComposeBackupProcess, this);
  }
}

void UiComposeView::StopComposeBackup()
{
  if (m_ComposeBackupInterval != 0)
  {
    m_ComposeBackupRunning = false;
    {
      std::unique_lock<std::mutex> lock(m_ComposeBackupMutex);
      m_ComposeBackupCond.notify_one();
    }
    m_ComposeBackupThread.join();
  }
}

void UiComposeView::ComposeBackupProcess()
{
  LOG_DEBUG("starting backup thread");
  while (m_ComposeBackupRunning)
  {
    bool timedOut = false;

    {
      std::unique_lock<std::mutex> lock(m_ComposeBackupMutex);
      if (m_ComposeBackupCond.wait_for(lock, std::chrono::seconds(m_ComposeBackupInterval)) ==
          std::cv_status::timeout)
      {
        timedOut = true;
      }
    }

    if (timedOut)
    {
      SmtpManager::Action smtpAction;
      smtpAction.m_IsCreateMessage = true;
      smtpAction.m_From = StrUtil::ToString(GetComposeStr(Ui::HeaderFrom));
      smtpAction.m_To = StrUtil::ToString(GetComposeStr(Ui::HeaderTo));
      smtpAction.m_Cc = StrUtil::ToString(GetComposeStr(Ui::HeaderCc));
      smtpAction.m_Bcc = StrUtil::ToString(GetComposeStr(Ui::HeaderBcc));
      smtpAction.m_Att = StrUtil::ToString(GetComposeStr(Ui::HeaderAtt));
      smtpAction.m_Subject = StrUtil::ToString(GetComposeStr(Ui::HeaderSub));
      smtpAction.m_Body = StrUtil::ToString(GetComposeBodyForSend());
      smtpAction.m_HtmlBody = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
      smtpAction.m_RefMsgId = m_ComposeHeaderRef;

      SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
      if (smtpResult.m_SmtpStatus == SmtpStatusOk)
      {
        OfflineQueue::PushComposeMessage(smtpResult.m_Message);
        LOG_DEBUG("backup thread message saved");
      }
      else
      {
        LOG_WARNING("backup thread message creation failed");
      }
    }
  }

  if (!m_ComposeBackupKeep)
  {
    OfflineQueue::PopComposeMessages();
  }

  LOG_DEBUG("stopping backup thread");
}

void UiComposeView::AddAttachmentPath(const std::string& p_Path)
{
  if (p_Path.empty()) return;

  std::string filepaths;
  const std::string& oldFilepaths = StrUtil::Trim(StrUtil::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine]));

  const std::string newPath = FileUtil::EscapePath(p_Path);
  if (oldFilepaths.empty())
  {
    filepaths = newPath;
  }
  else if (oldFilepaths[oldFilepaths.size() - 1] != ',')
  {
    filepaths = ", " + newPath;
  }
  else
  {
    filepaths = " " + newPath;
  }

  m_ComposeHeaderStr[m_ComposeHeaderLine] = StrUtil::ToWString(oldFilepaths + filepaths);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

void UiComposeView::AddAddress(const std::string& p_Address)
{
  std::string addAddress;
  const std::string& oldAddress = StrUtil::Trim(StrUtil::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine]));
  if (oldAddress.empty())
  {
    addAddress = p_Address;
  }
  else if (oldAddress[oldAddress.size() - 1] != ',')
  {
    addAddress = ", " + p_Address;
  }
  else
  {
    addAddress = " " + p_Address;
  }

  m_ComposeHeaderStr[m_ComposeHeaderLine] = StrUtil::ToWString(oldAddress + addAddress);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

void UiComposeView::SetAddress(const std::string& p_Address)
{
  m_ComposeHeaderStr[m_ComposeHeaderLine] = StrUtil::ToWString(p_Address);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

std::string UiComposeView::GetDefaultFrom()
{
  static const Contact defaultFrom(m_Model->GetAddress(), m_Name);
  return defaultFrom.ToString();
}

std::wstring UiComposeView::GetSignatureStr(bool p_NoPrefix /*= false*/)
{
  if (!m_Signature)
  {
    return std::wstring();
  }

  std::string signatureStr;
  static const std::string sigPriPath = AppUtil::GetApplicationDir() + "signature.txt";
  static const std::string sigSecPath = std::string(getenv("HOME")) + "/.signature";
  if (FileUtil::IsReadableFile(sigPriPath))
  {
    signatureStr = FileUtil::ReadFile(sigPriPath);
  }
  else if (FileUtil::IsReadableFile(sigSecPath))
  {
    signatureStr = FileUtil::ReadFile(sigSecPath);
  }
  else
  {
    LOG_WARNING("signature file missing");
    return std::wstring();
  }

  if (signatureStr.empty())
  {
    LOG_WARNING("signature file is empty");
    return std::wstring();
  }

  if (!p_NoPrefix)
  {
    signatureStr = "\n\n" + signatureStr; // prefix with line breaks
  }

  std::wstring signatureWStr = StrUtil::ToWString(signatureStr);
  StrUtil::StripCR(signatureWStr);

  return signatureWStr;
}

bool UiComposeView::HandleComposeKey(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    if (m_CancelWithoutConfirm || PromptYesNo("Cancel message (y/n)?"))
    {
      FileUtil::RmDir(FileUtil::GetPreviewTempDir());
      m_MessageListView->UpdateUidFromIndex(true /* p_UserTriggered */);
      SetLastStateOrMessageList();
      FileUtil::RmDir(m_ComposeTempDirectory);
      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeySend)
  {
    if (ComposedMessageIsValid(true /* p_ForSend */) &&
        (m_SendWithoutConfirm || PromptYesNo("Send message (y/n)?")))
    {
      FileUtil::RmDir(FileUtil::GetPreviewTempDir());
      SendComposedMessage();
      m_MessageListView->UpdateUidFromIndex(true /* p_UserTriggered */);
      if (m_ComposeDraftUid != 0)
      {
        SetState(Ui::StateViewMessageList);
      }
      else
      {
        SetLastStateOrMessageList();
      }

      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeyPostpone)
  {
    if (ComposedMessageIsValid(false /* p_ForSend */) &&
        (m_PostponeWithoutConfirm || PromptYesNo("Postpone message (y/n)?")))
    {
      FileUtil::RmDir(FileUtil::GetPreviewTempDir());
      UploadDraftMessage();
      m_MessageListView->UpdateUidFromIndex(true /* p_UserTriggered */);
      if (m_ComposeDraftUid != 0)
      {
        SetState(Ui::StateViewMessageList);
      }
      else
      {
        SetLastStateOrMessageList();
      }

      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeyExtEditor)
  {
    const std::string editorCmd = ExtUtil::GetEditorCmd();
    m_External->ExtEditor(editorCmd, m_ComposeMessageStr, m_ComposeMessagePos);
  }
  else if (p_Key == m_KeyRichHeader)
  {
    std::wstring from = GetComposeStr(Ui::HeaderFrom);
    std::wstring to = GetComposeStr(Ui::HeaderTo);
    std::wstring cc = GetComposeStr(Ui::HeaderCc);
    std::wstring bcc = GetComposeStr(Ui::HeaderBcc);
    std::wstring att = GetComposeStr(Ui::HeaderAtt);
    std::wstring sub = GetComposeStr(Ui::HeaderSub);

    m_ShowRichHeader = !m_ShowRichHeader;

    SetComposeStr(Ui::HeaderAll, L"");
    SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    SetComposeStr(Ui::HeaderTo, to);
    SetComposeStr(Ui::HeaderCc, cc);
    SetComposeStr(Ui::HeaderBcc, bcc);
    SetComposeStr(Ui::HeaderAtt, att);
    SetComposeStr(Ui::HeaderSub, sub);
  }
  else if (p_Key == m_KeyExtHtmlPreview)
  {
    if (m_CurrentMarkdownHtmlCompose)
    {
      std::string tempFilePath = FileUtil::GetPreviewTempDir() + "msg.html";
      std::string htmlStr = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
      FileUtil::WriteFile(tempFilePath, htmlStr);
      m_External->ExtHtmlPreview(tempFilePath);
    }
    else
    {
      SetDialogMessage("Markdown compose is not enabled");
    }
  }
  else if (p_Key == m_KeyToggleMarkdownCompose)
  {
    m_CurrentMarkdownHtmlCompose = !m_CurrentMarkdownHtmlCompose;
  }
  else if (p_Key == m_KeySpell)
  {
    const std::string spellCmd = ExtUtil::GetSpellCmd();
    if (!spellCmd.empty())
    {
      size_t quoteStartPos =
        !m_ComposeQuotedStart.empty() ? m_ComposeMessageStr.find(StrUtil::ToWString(m_ComposeQuotedStart))
                                      : std::string::npos;
      if (quoteStartPos == std::string::npos)
      {
        m_External->ExtEditor(spellCmd, m_ComposeMessageStr, m_ComposeMessagePos);
      }
      else
      {
        std::wstring messageComposed = m_ComposeMessageStr.substr(0, quoteStartPos);
        std::wstring messageQuoted = m_ComposeMessageStr.substr(quoteStartPos);
        m_External->ExtEditor(spellCmd, messageComposed, m_ComposeMessagePos);
        m_ComposeMessageStr = messageComposed + messageQuoted;
      }
    }
    else
    {
      SetDialogMessage("Spell command not found", true);
    }
  }
  else
  {
    return false;
  }

  return true;
}

void UiComposeView::FilePickerOrStateFileList()
{
  std::string filePickerCmd = ExtUtil::GetFilePickerCmd();
  if (filePickerCmd.empty())
  {
    SetState(Ui::StateFileList);
  }
  else
  {
    const std::vector<std::string> files = m_External->ExtFilePicker(filePickerCmd);
    for (const auto& file : files)
    {
      AddAttachmentPath(file);
    }
  }
}

bool UiComposeView::IsDrawnLast()
{
  // after help and dialog, as it shows the cursor
  return true;
}

void UiComposeView::OnEnter()
{
  if ((m_State == Ui::StateComposeMessage) || (m_State == Ui::StateComposeCopyMessage))
  {
    curs_set(1);
    SetComposeStr(Ui::HeaderAll, L"");
    SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeHeaderRef.clear();
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_IsComposeHeader = true;
    m_ComposeDraftUid = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    UiModel::Lock model(m_Model);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    if (m_ComposeMailto.empty() && ((folder == model->m_DraftsFolder) || (m_State == Ui::StateComposeCopyMessage)))
    {
      std::map<uint32_t, Header>& headers = model->m_Headers[folder];
      std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];

      std::map<uint32_t, Header>::iterator hit = headers.find(uid);
      std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
      if ((hit != headers.end()) && (bit != bodys.end()))
      {
        m_ComposeDraftUid = (m_State != Ui::StateComposeCopyMessage) ? uid : 0;

        Header& header = hit->second;
        Body& body = bit->second;

        std::string bodyText = body.GetTextPlain();
        if ((folder != model->m_DraftsFolder) && !bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
        {
          bodyText = bodyText.substr(0, bodyText.size() - 1);
        }

        m_ComposeMessageStr = StrUtil::ToWString(bodyText);
        StrUtil::StripCR(m_ComposeMessageStr);

        SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(header.GetFrom()));
        SetComposeStr(Ui::HeaderTo, StrUtil::ToWString(header.GetTo()));
        SetComposeStr(Ui::HeaderCc, StrUtil::ToWString(header.GetCc()));
        SetComposeStr(Ui::HeaderBcc, StrUtil::ToWString(header.GetBcc()));
        SetComposeStr(Ui::HeaderAtt, L"");
        SetComposeStr(Ui::HeaderSub, StrUtil::ToWString(header.GetSubject()));

        if (!GetComposeStr(Ui::HeaderBcc).empty())
        {
          m_ShowRichHeader = true;
        }

        int idx = 0;
        std::string tmppath = FileUtil::GetTempDirectory();
        const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
        for (auto& part : body.GetPartInfos())
        {
          if (!part.second.m_Filename.empty())
          {
            std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
            FileUtil::MkDir(tmpfiledir);
            std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

            FileUtil::WriteFile(tmpfilepath, partDatas.at(part.first));
            tmpfilepath = FileUtil::EscapePath(tmpfilepath);
            if (GetComposeStr(Ui::HeaderAtt).empty())
            {
              SetComposeStr(Ui::HeaderAtt, GetComposeStr(Ui::HeaderAtt) + StrUtil::ToWString(tmpfilepath));
            }
            else
            {
              SetComposeStr(Ui::HeaderAtt, GetComposeStr(Ui::HeaderAtt) + L", " + StrUtil::ToWString(tmpfilepath));
            }
          }
        }

        m_ComposeHeaderRef = header.GetMessageId();
        m_ComposeTempDirectory = tmppath;
      }
    }
    else
    {
      m_ComposeMessageStr = GetSignatureStr();
    }

    if (!m_ComposeMailto.empty())
    {
      SetComposeMailto(m_ComposeMailto);
      m_ComposeMailto.clear();
    }
  }
  else if ((m_State == Ui::StateReplyAllMessage) || (m_State == Ui::StateReplySenderMessage))
  {
    curs_set(1);
    SetComposeStr(Ui::HeaderAll, L"");
    SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 5 : 3;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    UiModel::Lock model(m_Model);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string bodyText = m_MessageView->GetBodyText(body);
      if (!bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
      {
        bodyText = bodyText.substr(0, bodyText.size() - 1);
      }

      std::string indentBodyText = TextUtil::AddIndent(bodyText, "> ");
      const int quotedMaxLen = 78;

      if (m_UnwrapQuotedLines)
      {
        indentBodyText = TextUtil::UnwrapQuotedLines(indentBodyText, quotedMaxLen);
      }

      std::string indentBody;
      if (m_ComposeLineWrap == Ui::LineWrapFormatFlowed)
      {
        indentBody = indentBodyText;
      }
      else
      {
        const bool processFlowed = m_RespectFormatFlowed && m_Plaintext && body.IsFormatFlowed();
        const bool outputFlowed = false;
        const bool quoteWrap = m_RewrapQuotedLines;
        const int expandTabSize = m_TabSize; // enabled
        std::vector<std::wstring> indentBodyLines =
          TextUtil::WordWrap(StrUtil::ToWString(indentBodyText), quotedMaxLen, processFlowed,
                             outputFlowed, quoteWrap, expandTabSize);
        indentBody = StrUtil::ToString(StrUtil::Join(indentBodyLines));
      }

      m_ComposeQuotedStart = "On " + header.GetDateTime() + " " + header.GetFrom() + " wrote:\n\n";
      if (!m_BottomReply)
      {
        m_ComposeMessageStr = GetSignatureStr() +
          StrUtil::ToWString("\n\n" + m_ComposeQuotedStart + indentBody);
        StrUtil::StripCR(m_ComposeMessageStr);
      }
      else
      {
        m_ComposeMessageStr =
          StrUtil::ToWString(m_ComposeQuotedStart + indentBody + "\n\n\n");
        StrUtil::StripCR(m_ComposeMessageStr);
        m_ComposeMessagePos = (int)m_ComposeMessageStr.size() - 1;
        int lineCount = StrUtil::Split(StrUtil::ToString(m_ComposeMessageStr), '\n').size();
        m_ComposeMessageOffsetY = std::max(lineCount - (m_MainWinHeight / 2), 0);
        m_ComposeMessageStr += GetSignatureStr();
      }

      {
        if (folder == model->m_SentFolder)
        {
          SetComposeStr(Ui::HeaderTo, StrUtil::ToWString(header.GetTo()));
        }
        else
        {
          if (!header.GetReplyTo().empty())
          {
            SetComposeStr(Ui::HeaderTo, StrUtil::ToWString(header.GetReplyTo()));
          }
          else
          {
            SetComposeStr(Ui::HeaderTo, StrUtil::ToWString(header.GetFrom()));
          }
        }

        if (m_State == Ui::StateReplyAllMessage)
        {
          std::vector<std::string> tos = MailUtil::SplitAddrs(header.GetTo());
          std::vector<std::string> ccs = MailUtil::SplitAddrs(header.GetCc());

          if (folder != model->m_SentFolder)
          {
            ccs.insert(ccs.end(), tos.begin(), tos.end());
          }

          std::string selfAddress = m_SmtpManager->GetAddress();
          for (auto it = ccs.begin(); it != ccs.end(); /* incremented in loop */)
          {
            it = ((it->find(selfAddress) == std::string::npos) &&
                  (it->find(header.GetFrom()) == std::string::npos)) ? std::next(it) : ccs.erase(it);
          }

          SetComposeStr(Ui::HeaderCc, StrUtil::ToWString(StrUtil::Join(ccs, ", ")));
        }
      }

      SetComposeStr(Ui::HeaderBcc, L"");
      SetComposeStr(Ui::HeaderAtt, L"");
      SetComposeStr(Ui::HeaderSub, StrUtil::ToWString(MailUtil::MakeReplySubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
    }

    m_IsComposeHeader = false;
  }
  else if (m_State == Ui::StateForwardMessage)
  {
    curs_set(1);
    SetComposeStr(Ui::HeaderAll, L"");
    SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    UiModel::Lock model(m_Model);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      int idx = 0;
      std::string tmppath = FileUtil::GetTempDirectory();
      const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
      for (auto& part : body.GetPartInfos())
      {
        if (!part.second.m_Filename.empty())
        {
          std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
          FileUtil::MkDir(tmpfiledir);
          std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

          FileUtil::WriteFile(tmpfilepath, partDatas.at(part.first));
          tmpfilepath = FileUtil::EscapePath(tmpfilepath);
          if (GetComposeStr(Ui::HeaderAtt).empty())
          {
            SetComposeStr(Ui::HeaderAtt,
                          GetComposeStr(Ui::HeaderAtt) + StrUtil::ToWString(tmpfilepath));
          }
          else
          {
            SetComposeStr(Ui::HeaderAtt,
                          GetComposeStr(Ui::HeaderAtt) + L", " + StrUtil::ToWString(tmpfilepath));
          }
        }
      }

      m_ComposeQuotedStart = "\n\n---------- Forwarded message ---------\n";
      m_ComposeMessageStr =
        GetSignatureStr() +
        StrUtil::ToWString(m_ComposeQuotedStart +
                           "From: " + header.GetFrom() + "\n"
                           "Date: " + header.GetDateTime() + "\n"
                           "Subject: " + header.GetSubject() + "\n"
                           "To: " + header.GetTo() + "\n");
      if (!header.GetReplyTo().empty())
      {
        m_ComposeMessageStr +=
          StrUtil::ToWString("Reply-To: " + header.GetReplyTo() + "\n");
      }

      if (!header.GetCc().empty())
      {
        m_ComposeMessageStr +=
          StrUtil::ToWString("Cc: " + header.GetCc() + "\n");
      }

      std::string bodyText = m_MessageView->GetBodyText(body);
      if (!bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
      {
        bodyText = bodyText.substr(0, bodyText.size() - 1);
      }

      m_ComposeMessageStr += StrUtil::ToWString("\n" + bodyText);
      StrUtil::StripCR(m_ComposeMessageStr);

      SetComposeStr(Ui::HeaderSub, StrUtil::ToWString(MailUtil::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
  else if (m_State == Ui::StateForwardAttachedMessage)
  {
    curs_set(1);
    SetComposeStr(Ui::HeaderAll, L"");
    SetComposeStr(Ui::HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    UiModel::Lock model(m_Model);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = model->m_Headers[folder];
    std::map<uint32_t, Body>& bodys = model->m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string tmppath = FileUtil::GetTempDirectory();
      std::string filename = header.GetSubject();
      StrUtil::RemoveNonAlphaNumSpace(filename);
      StrUtil::ReplaceString(filename, " ", "_");
      std::string filepath = tmppath + "/" + filename + ".eml";
      LOG_INFO("write to %s size %d", filepath.c_str(), body.GetData().size());
      FileUtil::WriteFile(filepath, body.GetData());

      SetComposeStr(Ui::HeaderAtt,
                    GetComposeStr(Ui::HeaderAtt) + StrUtil::ToWString(filepath));

      SetComposeStr(Ui::HeaderSub, StrUtil::ToWString(MailUtil::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
}

void UiComposeView::OnTerminate()
{
  if (m_ComposeBackupRunning)
  {
    // keep compose backup, for draft recovery at next start
    m_ComposeBackupKeep = true;
    StopComposeBackup();
  }
}
