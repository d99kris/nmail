// uicomposeview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

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

void Ui::DrawComposeMessage()
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

void Ui::ComposeMessageKeyHandler(int p_Key)
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
      if ((headerField == HeaderTo) || (headerField == HeaderCc) || (headerField == HeaderBcc))
      {
        SetState(StateAddressList);
      }
      else if (headerField == HeaderFrom)
      {
        SetState(StateFromAddressList);
      }
      else if (headerField == HeaderAtt)
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
    AsyncUiRequest(UiRequestDrawAll);
  }
  else
  {
    DrawAll();
  }
}

bool Ui::ComposedMessageIsValid(bool p_ForSend)
{
  std::string addrs =
    StrUtil::ToString(GetComposeStr(HeaderTo)) +
    StrUtil::ToString(GetComposeStr(HeaderCc)) +
    StrUtil::ToString(GetComposeStr(HeaderBcc));
  if (p_ForSend && (addrs.find("@") == std::string::npos))
  {
    SetDialogMessage("No recipients specified");
    return false;
  }

  std::vector<std::string> atts = FileUtil::SplitPaths(StrUtil::ToString(GetComposeStr(HeaderAtt)));
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

  std::string subject = StrUtil::ToString(GetComposeStr(HeaderSub));
  if (p_ForSend && subject.empty())
  {
    if (!Ui::PromptYesNo("No subject specified, continue (y/n)?"))
    {
      return false;
    }
  }

  return true;
}

void Ui::SendComposedMessage()
{
  SmtpManager::Action smtpAction;
  smtpAction.m_From = StrUtil::ToString(GetComposeStr(HeaderFrom));
  smtpAction.m_To = StrUtil::ToString(GetComposeStr(HeaderTo));
  smtpAction.m_Cc = StrUtil::ToString(GetComposeStr(HeaderCc));
  smtpAction.m_Bcc = StrUtil::ToString(GetComposeStr(HeaderBcc));
  smtpAction.m_Att = StrUtil::ToString(GetComposeStr(HeaderAtt));
  smtpAction.m_Subject = StrUtil::ToString(GetComposeStr(HeaderSub));
  smtpAction.m_Body = StrUtil::ToString(GetComposeBodyForSend());
  smtpAction.m_HtmlBody = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
  smtpAction.m_RefMsgId = m_ComposeHeaderRef;
  smtpAction.m_ComposeTempDirectory = m_ComposeTempDirectory;
  smtpAction.m_ComposeDraftUid = m_ComposeDraftUid;
  smtpAction.m_FormatFlowed = (m_ComposeLineWrap == LineWrapFormatFlowed);

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

void Ui::UploadDraftMessage()
{
  if (!m_DraftsFolder.empty())
  {
    SmtpManager::Action smtpAction;
    smtpAction.m_IsCreateMessage = true;
    smtpAction.m_From = StrUtil::ToString(GetComposeStr(HeaderFrom));
    smtpAction.m_To = StrUtil::ToString(GetComposeStr(HeaderTo));
    smtpAction.m_Cc = StrUtil::ToString(GetComposeStr(HeaderCc));
    smtpAction.m_Bcc = StrUtil::ToString(GetComposeStr(HeaderBcc));
    smtpAction.m_Att = StrUtil::ToString(GetComposeStr(HeaderAtt));
    smtpAction.m_Subject = StrUtil::ToString(GetComposeStr(HeaderSub));
    smtpAction.m_Body = StrUtil::ToString(GetComposeBodyForSend());
    smtpAction.m_HtmlBody = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
    smtpAction.m_RefMsgId = m_ComposeHeaderRef;

    SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
    if (smtpResult.m_SmtpStatus == SmtpStatusOk)
    {
      ImapManager::Action imapAction;
      imapAction.m_UploadDraft = true;
      imapAction.m_Folder = m_DraftsFolder;
      imapAction.m_Msg = smtpResult.m_Message;

      if (IsConnected())
      {
        m_ImapManager->AsyncAction(imapAction);

        if (m_ComposeDraftUid != 0)
        {
          MoveMessages(std::set<uint32_t>({ m_ComposeDraftUid }), m_DraftsFolder, m_TrashFolder);
        }

        m_HasRequestedUids[m_DraftsFolder] = false;
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

int Ui::GetComposeMessageColumn()
{
  // display column of current position, which differs from position in line for wide characters
  if (m_ComposeMessageWrapLine >= (int)m_ComposeMessageLines.size()) return 0;

  const std::wstring& line = m_ComposeMessageLines[m_ComposeMessageWrapLine];
  return TextWrap::GetWidth(line.substr(0, m_ComposeMessageWrapPos));
}

void Ui::SetComposeMessagePos(int p_WrapLine, int p_Column)
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

void Ui::ComposeMessagePrevLine()
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

void Ui::ComposeMessageNextLine()
{
  if (m_ComposeMessagePos < (int)m_ComposeMessageStr.size())
  {
    SetComposeMessagePos(m_ComposeMessageWrapLine + 1, GetComposeMessageColumn());
  }
}

// compose fields from mailto:addrs?cc=..&bcc=..&subject=..&body=.. (rfc 6068)
void Ui::SetComposeMailto(const std::string& p_Url)
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
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    m_ComposeHeaderLine = 1;
  }

  SetComposeStr(HeaderTo, StrUtil::ToWString(StrUtil::Join(tos, ", ")));
  SetComposeStr(HeaderCc, StrUtil::ToWString(StrUtil::Join(ccs, ", ")));
  SetComposeStr(HeaderBcc, StrUtil::ToWString(StrUtil::Join(bccs, ", ")));
  SetComposeStr(HeaderSub, StrUtil::ToWString(subject));
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

std::wstring Ui::GetComposeStr(int p_HeaderField)
{
  if (m_ShowRichHeader)
  {
    return m_ComposeHeaderStr[p_HeaderField];
  }
  else
  {
    switch (p_HeaderField)
    {
      case HeaderTo: return m_ComposeHeaderStr[0];

      case HeaderCc: return m_ComposeHeaderStr[1];

      case HeaderAtt: return m_ComposeHeaderStr[2];

      case HeaderSub: return m_ComposeHeaderStr[3];

      default: break;
    }
  }

  return L"";
}

void Ui::SetComposeStr(int p_HeaderField, const std::wstring& p_Str)
{
  if (p_HeaderField == HeaderAll)
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
        case HeaderTo:
          m_ComposeHeaderStr[0] = p_Str;
          break;

        case HeaderCc:
          m_ComposeHeaderStr[1] = p_Str;
          break;

        case HeaderAtt:
          m_ComposeHeaderStr[2] = p_Str;
          break;

        case HeaderSub:
          m_ComposeHeaderStr[3] = p_Str;
          break;

        default:
          break;
      }
    }
  }
}

std::wstring Ui::GetComposeBodyForSend()
{
  if (m_ComposeLineWrap == LineWrapNone)
  {
    return m_ComposeMessageStr;
  }
  else if (m_ComposeLineWrap == LineWrapFormatFlowed)
  {
    const bool processFlowed = m_CurrentMessageProcessFlowed;
    const bool outputFlowed = true;
    const bool quoteWrap = m_RewrapQuotedLines;
    const int expandTabSize = 0; // disabled
    std::vector<std::wstring> indentBodyLines =
      TextUtil::WordWrap(m_ComposeMessageStr, 72, processFlowed, outputFlowed, quoteWrap, expandTabSize);
    return StrUtil::Join(indentBodyLines);
  }
  else if (m_ComposeLineWrap == LineWrapHardWrap)
  {
    return StrUtil::Join(m_ComposeMessageLines);
  }
  else
  {
    LOG_WARNING("invalid line wrap %d", m_ComposeLineWrap);
  }
  return L"";
}

int Ui::GetCurrentHeaderField()
{
  if (m_ShowRichHeader)
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return HeaderFrom;

      case 1: return HeaderTo;

      case 2: return HeaderCc;

      case 3: return HeaderBcc;

      case 4: return HeaderAtt;

      case 5: return HeaderSub;

      default: break;
    }
  }
  else
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return HeaderTo;

      case 1: return HeaderCc;

      case 2: return HeaderAtt;

      case 3: return HeaderSub;

      default: break;
    }
  }

  return HeaderAll;
}

std::string Ui::MakeHtmlPart(const std::string& p_Text)
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

std::string Ui::MakeHtmlPartCustomSig(const std::string& p_Text)
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

void Ui::StartComposeBackup()
{
  if (m_ComposeBackupInterval != 0)
  {
    m_ComposeBackupRunning = true;
    m_ComposeBackupThread = std::thread(&Ui::ComposeBackupProcess, this);
  }
}

void Ui::StopComposeBackup()
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

void Ui::ComposeBackupProcess()
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
      smtpAction.m_From = StrUtil::ToString(GetComposeStr(HeaderFrom));
      smtpAction.m_To = StrUtil::ToString(GetComposeStr(HeaderTo));
      smtpAction.m_Cc = StrUtil::ToString(GetComposeStr(HeaderCc));
      smtpAction.m_Bcc = StrUtil::ToString(GetComposeStr(HeaderBcc));
      smtpAction.m_Att = StrUtil::ToString(GetComposeStr(HeaderAtt));
      smtpAction.m_Subject = StrUtil::ToString(GetComposeStr(HeaderSub));
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

void Ui::AddAttachmentPath(const std::string& p_Path)
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

void Ui::AddAddress(const std::string& p_Address)
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

void Ui::SetAddress(const std::string& p_Address)
{
  m_ComposeHeaderStr[m_ComposeHeaderLine] = StrUtil::ToWString(p_Address);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

std::string Ui::GetDefaultFrom()
{
  static const Contact defaultFrom(m_Address, m_Name);
  return defaultFrom.ToString();
}

std::wstring Ui::GetSignatureStr(bool p_NoPrefix /*= false*/)
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

bool Ui::HandleComposeKey(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    if (m_CancelWithoutConfirm || Ui::PromptYesNo("Cancel message (y/n)?"))
    {
      FileUtil::RmDir(FileUtil::GetPreviewTempDir());
      UpdateUidFromIndex(true /* p_UserTriggered */);
      SetLastStateOrMessageList();
      FileUtil::RmDir(m_ComposeTempDirectory);
      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeySend)
  {
    if (ComposedMessageIsValid(true /* p_ForSend */) &&
        (m_SendWithoutConfirm || Ui::PromptYesNo("Send message (y/n)?")))
    {
      FileUtil::RmDir(FileUtil::GetPreviewTempDir());
      SendComposedMessage();
      UpdateUidFromIndex(true /* p_UserTriggered */);
      if (m_ComposeDraftUid != 0)
      {
        SetState(StateViewMessageList);
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
        (m_PostponeWithoutConfirm || Ui::PromptYesNo("Postpone message (y/n)?")))
    {
      FileUtil::RmDir(FileUtil::GetPreviewTempDir());
      UploadDraftMessage();
      UpdateUidFromIndex(true /* p_UserTriggered */);
      if (m_ComposeDraftUid != 0)
      {
        SetState(StateViewMessageList);
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
    ExtEditor(editorCmd, m_ComposeMessageStr, m_ComposeMessagePos);
  }
  else if (p_Key == m_KeyRichHeader)
  {
    std::wstring from = GetComposeStr(HeaderFrom);
    std::wstring to = GetComposeStr(HeaderTo);
    std::wstring cc = GetComposeStr(HeaderCc);
    std::wstring bcc = GetComposeStr(HeaderBcc);
    std::wstring att = GetComposeStr(HeaderAtt);
    std::wstring sub = GetComposeStr(HeaderSub);

    m_ShowRichHeader = !m_ShowRichHeader;

    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, StrUtil::ToWString(GetDefaultFrom()));
    SetComposeStr(HeaderTo, to);
    SetComposeStr(HeaderCc, cc);
    SetComposeStr(HeaderBcc, bcc);
    SetComposeStr(HeaderAtt, att);
    SetComposeStr(HeaderSub, sub);
  }
  else if (p_Key == m_KeyExtHtmlPreview)
  {
    if (m_CurrentMarkdownHtmlCompose)
    {
      std::string tempFilePath = FileUtil::GetPreviewTempDir() + "msg.html";
      std::string htmlStr = MakeHtmlPart(StrUtil::ToString(m_ComposeMessageStr));
      FileUtil::WriteFile(tempFilePath, htmlStr);
      ExtHtmlPreview(tempFilePath);
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
        ExtEditor(spellCmd, m_ComposeMessageStr, m_ComposeMessagePos);
      }
      else
      {
        std::wstring messageComposed = m_ComposeMessageStr.substr(0, quoteStartPos);
        std::wstring messageQuoted = m_ComposeMessageStr.substr(quoteStartPos);
        ExtEditor(spellCmd, messageComposed, m_ComposeMessagePos);
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
